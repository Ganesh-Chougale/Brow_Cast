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

// Map to store active sessions
const sessions = new Map();

/**
 * Helper function to get the local IP address of the machine.
 * Filters for IPv4, non-internal addresses.
 */
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

const SERVER_PORT = process.env.PORT || 8080;
const SERVER_IP_ADDRESS = getLocalIpAddress(); // Get the IP address dynamically

// WebSocket connection handling
wss.on('connection', (ws, req) => {
    const parsedUrl = url.parse(req.url, true);
    const path = parsedUrl.pathname;
    const sessionId = parsedUrl.query.sessionId;

    if (!sessionId) {
        console.warn('Connection attempt without sessionId. Closing connection.');
        ws.close(1008, 'Session ID required');
        return;
    }

    if (!sessions.has(sessionId)) {
        sessions.set(sessionId, { agent: null, viewers: new Set() });
        console.log(`New session created: ${sessionId}`);
    }
    const currentSession = sessions.get(sessionId);

    if (path === '/agent') {
        if (currentSession.agent && currentSession.agent.readyState === WebSocket.OPEN) {
            console.warn(`Agent already connected for session ${sessionId}. Closing new agent connection.`);
            ws.close(1008, 'Agent already connected to this session');
            return;
        }

        currentSession.agent = ws;
        console.log(`Agent connected to session: ${sessionId}`);

        currentSession.viewers.forEach(viewerWs => {
            if (viewerWs.readyState === WebSocket.OPEN) {
                viewerWs.send(JSON.stringify({ type: 'agent_status', connected: true }));
            }
        });

        ws.on('message', message => {
            currentSession.viewers.forEach(viewerWs => {
                if (viewerWs.readyState === WebSocket.OPEN) {
                    viewerWs.send(message.toString());
                }
            });
        });

        ws.on('close', () => {
            console.log(`Agent disconnected from session: ${sessionId}`);
            currentSession.agent = null;

            currentSession.viewers.forEach(viewerWs => {
                if (viewerWs.readyState === WebSocket.OPEN) {
                    viewerWs.send(JSON.stringify({ type: 'agent_status', connected: false }));
                }
            });

            cleanupSessionIfEmpty(sessionId);
        });

        ws.on('error', error => {
            console.error(`Agent WebSocket Error for session ${sessionId}:`, error);
        });

    } else if (path === '/viewer') {
        currentSession.viewers.add(ws);
        console.log(`Viewer connected to session: ${sessionId}. Total viewers: ${currentSession.viewers.size}`);

        if (currentSession.agent && currentSession.agent.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'agent_status', connected: true }));
        } else {
            ws.send(JSON.stringify({ type: 'agent_status', connected: false }));
        }

        ws.on('message', message => {
            const inputMessage = message.toString();
            if (currentSession.agent && currentSession.agent.readyState === WebSocket.OPEN) {
                currentSession.agent.send(inputMessage);
            } else {
                console.warn(`Received input for session ${sessionId}, but no agent is connected.`);
            }
        });

        ws.on('close', () => {
            currentSession.viewers.delete(ws);
            console.log(`Viewer disconnected from session: ${sessionId}. Total viewers: ${currentSession.viewers.size}`);
            
            cleanupSessionIfEmpty(sessionId);
        });

        ws.on('error', error => {
            console.error(`Viewer WebSocket Error for session ${sessionId}:`, error);
        });

    } else {
        console.warn(`Unknown connection path: ${path}. Closing connection.`);
        ws.close(1000, 'Unknown path');
    }
});

/**
 * Cleans up a session from the map if it has no connected agent and no connected viewers.
 */
function cleanupSessionIfEmpty(sessionId) {
    const session = sessions.get(sessionId);
    if (session && !session.agent && session.viewers.size === 0) {
        sessions.delete(sessionId);
        console.log(`Session ${sessionId} cleaned up (no agent or viewers).`);
    }
}

// Start the HTTP server
server.listen(SERVER_PORT, () => {
    console.log(`Server running on http://${SERVER_IP_ADDRESS}:${SERVER_PORT}`);
    console.log(`Machine IP: ${SERVER_IP_ADDRESS}`);
});
