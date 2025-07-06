const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const url = require('url');
const path = require('path');
const os = require('os'); // Node.js built-in module for OS-related utility methods

const app = express();
const server = http.createServer(app);

// Serve static files from the 'web' directory, located one level up from 'server'
app.use(express.static(path.join(__dirname, '../web')));

// Create a WebSocket server instance linked to the HTTP server
const wss = new WebSocket.Server({ server });

// Map to store active sessions with additional metadata
const sessions = new Map();

// Track connection state
const connectionState = {
    totalConnections: 0,
    activeConnections: 0,
    agentConnections: 0,
    viewerConnections: 0
};

/**
 * Helper function to log connection statistics
 */
function logConnectionStats() {
    console.log('\n=== Connection Statistics ===');
    console.log(`Total connections: ${connectionState.totalConnections}`);
    console.log(`Active connections: ${connectionState.activeConnections}`);
    console.log(`Agent connections: ${connectionState.agentConnections}`);
    console.log(`Viewer connections: ${connectionState.viewerConnections}`);
    console.log(`Active sessions: ${sessions.size}`);
    console.log('============================\n');
}

// Log connection stats every 30 seconds
setInterval(logConnectionStats, 30000);

// Function to calculate optimal scale
function calculateOptimalScale(agentScreen, clientScreen) {
    // Get the available width and height from client
    const clientAvailWidth = clientScreen.availWidth || clientScreen.windowWidth;
    const clientAvailHeight = clientScreen.availHeight || clientScreen.windowHeight;
    
    // Calculate the maximum scale that fits the client's available space
    const scaleX = clientAvailWidth / agentScreen.width;
    const scaleY = clientAvailHeight / agentScreen.height;
    
    // Use the smaller scale to maintain aspect ratio
    let scale = Math.min(scaleX, scaleY);
    
    // Apply DPI scaling if available
    if (clientScreen.devicePixelRatio && agentScreen.dpi) {
        // Convert devicePixelRatio to DPI (96 is standard CSS DPI)
        const clientDpi = clientScreen.devicePixelRatio * 96;
        const dpiRatio = clientDpi / agentScreen.dpi;
        scale *= dpiRatio;
    }
    
    // Ensure scale is within reasonable bounds
    return Math.max(0.1, Math.min(scale, 2.0));
}

// Handle WebSocket connections
wss.on('connection', (ws, req) => {
    connectionState.totalConnections++;
    connectionState.activeConnections++;
    
    const clientIp = req.connection.remoteAddress;
    console.log(`\n=== New WebSocket Connection (${connectionState.totalConnections}) ===`);
    console.log(`Client IP: ${clientIp}`);
    console.log(`Request URL: ${req.url}`);
    console.log(`Active connections: ${connectionState.activeConnections}`);

    const parsedUrl = url.parse(req.url, true);
    const path = parsedUrl.pathname;
    const sessionId = parsedUrl.query.sessionId;
    
    console.log(`Path: ${path}, Session ID: ${sessionId}`);

    if (!sessionId) {
        const errorMsg = 'Connection attempt without sessionId. Closing connection.';
        console.warn(errorMsg);
        ws.close(1008, errorMsg);
        connectionState.activeConnections--;
        return;
    }

    if (!sessions.has(sessionId)) {
        sessions.set(sessionId, { 
            agent: null, 
            viewers: new Map(), 
            agentScreen: null,
            clientScreens: new Map(),
            createdAt: new Date().toISOString(),
            lastActivity: new Date().toISOString()
        });
        console.log(`New session created: ${sessionId}`);
    }
    
    const session = sessions.get(sessionId);
    session.lastActivity = new Date().toISOString();

    // Handle agent connections
    if (path === '/agent') {
        handleAgentConnection(ws, req, sessionId, session);
    } 
    // Handle viewer connections
    else if (path === '/viewer') {
        handleViewerConnection(ws, req, sessionId, session);
    } 
    // Handle unknown paths
    else {
        const errorMsg = `Unknown connection path: ${path}`;
        console.warn(errorMsg);
        ws.close(1000, errorMsg);
        connectionState.activeConnections--;
    }
});

/**
 * Handles agent WebSocket connections
 */
function handleAgentConnection(ws, req, sessionId, session) {
    console.log(`Agent connection attempt for session: ${sessionId}`);
    
    if (session.agent && session.agent.readyState === WebSocket.OPEN) {
        const errorMsg = `Agent already connected for session ${sessionId}`;
        console.warn(errorMsg);
        ws.close(1008, errorMsg);
        connectionState.activeConnections--;
        return;
    }

    // Store the agent connection
    session.agent = ws;
    connectionState.agentConnections++;
    
    console.log(`Agent connected to session: ${sessionId}`);
    console.log(`Agent WebSocket state: ${ws.readyState}`);
    console.log(`Total agent connections: ${connectionState.agentConnections}`);

    // Notify all viewers that an agent has connected
    session.viewers.forEach(({ ws: viewerWs }) => {
        if (viewerWs.readyState === WebSocket.OPEN) {
            viewerWs.send(JSON.stringify({ type: 'agent_status', connected: true }));
        }
    });

    // Handle incoming messages from agent
    ws.on('message', message => {
        session.lastActivity = new Date().toISOString();
        
        try {
            const msg = JSON.parse(message.toString());
            
            // Handle screen info from agent
            if (msg.type === 'screen_info') {
                session.agentScreen = {
                    width: msg.width,
                    height: msg.height,
                    dpi: msg.dpi || 96
                };
                console.log(`Agent screen info: ${msg.width}x${msg.height} @ ${session.agentScreen.dpi} DPI`);
                
                // If we have client screens, update their scales
                session.clientScreens.forEach((clientScreen, clientWs) => {
                    if (clientWs.readyState === WebSocket.OPEN) {
                        const scale = calculateOptimalScale(session.agentScreen, clientScreen);
                        clientWs.send(JSON.stringify({
                            type: 'set_scale',
                            scale: scale,
                            originalWidth: session.agentScreen.width,
                            originalHeight: session.agentScreen.height
                        }));
                    }
                });
            }
            
            // Forward other messages to viewers
            session.viewers.forEach(({ ws: viewerWs }) => {
                if (viewerWs.readyState === WebSocket.OPEN) {
                    viewerWs.send(JSON.stringify({
                        ...msg,
                        originalWidth: session.agentScreen?.width,
                        originalHeight: session.agentScreen?.height
                    }));
                }
            });
        } catch (e) {
            console.error('Error processing agent message:', e);
        }
    });

    // Handle agent disconnection
    ws.on('close', () => {
        console.log(`Agent disconnected from session: ${sessionId}`);
        session.agent = null;
        connectionState.agentConnections--;
        connectionState.activeConnections--;
        
        // Notify all viewers that the agent has disconnected
        session.viewers.forEach(({ ws: viewerWs }) => {
            if (viewerWs.readyState === WebSocket.OPEN) {
                viewerWs.send(JSON.stringify({ type: 'agent_status', connected: false }));
            }
        });

        cleanupSessionIfEmpty(sessionId);
        logConnectionStats();
    });

    // Handle agent connection errors
    ws.on('error', error => {
        console.error(`Agent WebSocket error for session ${sessionId}:`, error);
        connectionState.activeConnections--;
        connectionState.agentConnections--;
        logConnectionStats();
    });

    // Set up ping-pong to detect disconnections
    setupPingPong(ws, sessionId, 'agent');
}

/**
 * Handles viewer WebSocket connections
 */
function handleViewerConnection(ws, req, sessionId, session) {
    console.log(`Viewer connection attempt for session: ${sessionId}`);
    
    // Generate a unique ID for this viewer
    const viewerId = Date.now().toString();
    const viewerInfo = { ws, id: viewerId, screenInfo: null };
    
    session.viewers.set(viewerId, viewerInfo);
    connectionState.viewerConnections++;
    
    console.log(`Viewer ${viewerId} connected to session: ${sessionId}. Total viewers: ${session.viewers.size}`);
    console.log(`Total viewer connections: ${connectionState.viewerConnections}`);

    // Handle client info message
    ws.on('message', message => {
        session.lastActivity = new Date().toISOString();
        
        try {
            const msg = JSON.parse(message.toString());
            
            // Handle client screen info
            if (msg.type === 'client_info') {
                viewerInfo.screenInfo = msg.screen;
                session.clientScreens.set(ws, viewerInfo.screenInfo);
                console.log(`Viewer ${viewerId} screen info: ${msg.screen.width}x${msg.screen.height} (device pixel ratio: ${msg.screen.devicePixelRatio || 1})`);
                
                // If we have agent screen info, calculate and send optimal scaling
                if (session.agentScreen) {
                    const scale = calculateOptimalScale(session.agentScreen, viewerInfo.screenInfo);
                    ws.send(JSON.stringify({
                        type: 'set_scale',
                        scale: scale,
                        originalWidth: session.agentScreen.width,
                        originalHeight: session.agentScreen.height
                    }));
                }
                return;
            }
            
            // Forward input events to agent
            if (session.agent && session.agent.readyState === WebSocket.OPEN) {
                session.agent.send(message);
            } else {
                console.warn(`Received input for session ${sessionId}, but no agent is connected.`);
            }
        } catch (e) {
            console.error('Error processing viewer message:', e);
        }
    });

    // Notify viewer if an agent is already connected
    if (session.agent && session.agent.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ type: 'agent_status', connected: true }));
    } else {
        ws.send(JSON.stringify({ type: 'agent_status', connected: false }));
    }

    // Handle viewer disconnection
    ws.on('close', () => {
        session.viewers.delete(viewerId);
        session.clientScreens.delete(ws);
        connectionState.viewerConnections--;
        connectionState.activeConnections--;
        console.log(`Viewer ${viewerId} disconnected from session: ${sessionId}. Remaining viewers: ${session.viewers.size}`);
        cleanupSessionIfEmpty(sessionId);
        logConnectionStats();
    });

    // Handle viewer connection errors
    ws.on('error', error => {
        console.error(`Viewer WebSocket error for session ${sessionId}:`, error);
        session.viewers.delete(viewerId);
        session.clientScreens.delete(ws);
        connectionState.activeConnections--;
        connectionState.viewerConnections--;
        logConnectionStats();
    });

    // Set up ping-pong to detect disconnections
    setupPingPong(ws, sessionId, 'viewer');
}

/**
 * Calculates the optimal scale for the viewer based on agent and viewer screen info
 */
function calculateOptimalScale(agentScreen, viewerScreen) {
    // Calculate scale factors for width and height
    const scaleX = (viewerScreen.windowWidth * 0.9) / agentScreen.width;
    const scaleY = (viewerScreen.windowHeight * 0.9) / agentScreen.height;
    
    // Use the smaller scale to ensure the entire screen is visible
    const scale = Math.min(scaleX, scaleY);
    
    // Limit the scale to a reasonable range (e.g., between 0.1 and 2.0)
    return Math.max(0.1, Math.min(scale, 2.0));
}

/**
 * Sets up ping-pong mechanism for WebSocket connection
 */
function setupPingPong(ws, sessionId, connectionType) {
    let isAlive = true;
    
    // Set up ping interval
    const pingInterval = setInterval(() => {
        if (ws.readyState === WebSocket.OPEN) {
            if (!isAlive) {
                console.warn(`No pong received from ${connectionType} ${sessionId}, terminating connection`);
                ws.terminate();
                return clearInterval(pingInterval);
            }
            
            isAlive = false;
            ws.ping(() => {});
        } else {
            clearInterval(pingInterval);
        }
    }, 30000);
    
    // Handle pong responses
    ws.on('pong', () => {
        isAlive = true;
        console.log(`Received pong from ${connectionType} ${sessionId}`);
    });
    
    // Clean up on close
    ws.on('close', () => {
        clearInterval(pingInterval);
    });
}

/**
 * Cleans up a session from the map if it has no connected agent and no connected viewers.
 */
function cleanupSessionIfEmpty(sessionId) {
    const session = sessions.get(sessionId);
    if (session && !session.agent && session.viewers.size === 0) {
        sessions.delete(sessionId);
        console.log(`Session ${sessionId} cleaned up (no agent or viewers).`);
        logConnectionStats();
    }
}

const SERVER_PORT = process.env.PORT || 8080;
const SERVER_IP_ADDRESS = getLocalIpAddress(); // Get the IP address dynamically

// Helper function to get the local IP address of the machine.
function getLocalIpAddress() {
    const interfaces = os.networkInterfaces();
    for (const name in interfaces) {
        for (const iface of interfaces[name]) {
            // Skip over internal (i.e. 127.0.0.1) and non-IPv4 addresses
            if (iface.family === 'IPv4' && !iface.internal) {
                return iface.address;
            }
        }
    }
    return 'localhost'; // Fallback to localhost if no suitable IP found
}

// Function to get all local IP addresses
function getAllLocalIpAddresses() {
    const interfaces = os.networkInterfaces();
    const addresses = [];
    
    Object.keys(interfaces).forEach(ifaceName => {
        interfaces[ifaceName].forEach(iface => {
            // Skip over internal (i.e. 127.0.0.1) and non-IPv4 addresses
            if (iface.family === 'IPv4' && !iface.internal) {
                addresses.push({
                    name: ifaceName,
                    address: iface.address,
                    mac: iface.mac,
                    internal: iface.internal
                });
            }
        });
    });
    
    return addresses;
}

// Start the HTTP server
server.listen(SERVER_PORT, '0.0.0.0', () => {
    const interfaces = getAllLocalIpAddresses();
    
    console.log('\n=== Remote Share Server ===');
    console.log(`\nServer is running on port ${SERVER_PORT}`);
    console.log('\nAvailable network interfaces:');
    console.log('----------------------------');
    
    // Display localhost URL
    console.log('Local access:');
    console.log(`  • http://localhost:${SERVER_PORT}`);
    
    // Display network interface URLs
    if (interfaces.length > 0) {
        console.log('\nNetwork access:');
        interfaces.forEach(iface => {
            console.log(`  • ${iface.name}: http://${iface.address}:${SERVER_PORT}`);
        });
    } else {
        console.log('\nNo network interfaces found. Only localhost access is available.');
    }
    
    console.log('\nTo share your screen, connect the agent and then open any of the above URLs in a browser.');
    console.log('----------------------------\n');
    
    // Log initial connection statistics
    logConnectionStats();
});
