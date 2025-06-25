    const express = require('express');
    const WebSocket = require('ws');
    const http = require('http');
    const path = require('path');

    const app = express();
    const server = http.createServer(app);
    const wss = new WebSocket.Server({ server });

    const PORT = process.env.PORT || 8080;

    // Serve static files from the 'Web' directory
    app.use(express.static(path.join(__dirname, '../Web')));

    const clients = {
        agent: null, // Stores the WebSocket connection for the agent
        viewers: new Set() // Stores a set of WebSocket connections for viewers
    };

    wss.on('connection', (ws, req) => {
        // Determine the type of connection based on the URL path
        const isAgent = req.url === '/agent';
        const isViewer = req.url === '/viewer';

        if (isAgent) {
            console.log('Agent connected');
            // Ensure only one agent can be connected at a time
            if (clients.agent && clients.agent.readyState === WebSocket.OPEN) {
                console.log('Another agent already connected. Closing new agent connection.');
                ws.close(1008, 'Another agent is already connected.'); // 1008: Policy Violation
                return;
            }
            clients.agent = ws; // Set the current agent connection
            // Notify all active viewers that the agent is now connected
            broadcastToViewers(JSON.stringify({ type: 'agent_status', connected: true }));

            // Handle messages coming from the agent (which should be screen frames)
            ws.on('message', (message) => {
                broadcastToViewers(message); // Broadcast agent's messages to all viewers
            });

            ws.on('close', () => {
                console.log('Agent disconnected');
                clients.agent = null; // Clear the agent connection on close
                // Notify viewers that the agent has disconnected
                broadcastToViewers(JSON.stringify({ type: 'agent_status', connected: false }));
            });

            ws.on('error', (error) => {
                console.error('Agent WebSocket error:', error);
                clients.agent = null; // Clear agent on error as well
                broadcastToViewers(JSON.stringify({ type: 'agent_status', connected: false }));
            });
        } else if (isViewer) {
            console.log('Viewer connected');
            clients.viewers.add(ws); // Add the new viewer to the set

            // Inform the newly connected viewer about the current agent's status
            if (clients.agent && clients.agent.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({ type: 'agent_status', connected: true }));
            } else {
                ws.send(JSON.stringify({ type: 'agent_status', connected: false }));
            }

            ws.on('close', () => {
                console.log('Viewer disconnected');
                clients.viewers.delete(ws); // Remove the viewer from the set on close
            });

            ws.on('error', (error) => {
                console.error('Viewer WebSocket error:', error);
                clients.viewers.delete(ws); // Remove viewer on error
            });
        } else {
            // Log and close connections that don't match expected paths
            console.log(`Unknown connection attempt from: ${req.url}. Closing.`);
            ws.close(1000, 'Unknown connection type'); // 1000: Normal Closure
        }
    });

    // Helper function to send a message to all connected viewers
    function broadcastToViewers(message) {
        clients.viewers.forEach((viewer) => {
            if (viewer.readyState === WebSocket.OPEN) {
                viewer.send(message);
            }
        });
    }

    // Graceful shutdown on SIGINT (Ctrl+C)
    process.on('SIGINT', () => {
        console.log('Shutting down server...');
        if (clients.agent) {
            clients.agent.close();
        }
        clients.viewers.forEach(viewer => viewer.close());
        server.close(() => {
            console.log('Server shut down');
            process.exit(0);
        });
    });

    // Start the HTTP and WebSocket server
    server.listen(PORT, () => {
        console.log(`Server running on http://localhost:${PORT}`);
        console.log(`Web client available at http://localhost:${PORT}/index.html`);
    });
    