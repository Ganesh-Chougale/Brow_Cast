const express = require('express');
const WebSocket = require('ws');
const http = require('http');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

const PORT = process.env.PORT || 8080;

app.use(express.static('../Web')); // Serves your web client files

// Store active web clients (browsers)
const webClients = new Set();
let agentWs = null; // Store a reference to the agent's WebSocket

wss.on('connection', ws => {
    // Determine if the connecting client is the agent or a web browser.
    // For a simple setup, we'll assume the first connection from the agent.
    // In a more complex app, you might use a handshake or specific endpoint.

    // If no agent is connected, this is likely our agent
    if (!agentWs) {
        agentWs = ws;
        console.log('Client connected (Agent)');

        ws.on('message', message => {
            // console.log("Received a frame from the agent."); // Keep for debugging if needed, but it's very verbose
            // When a message (frame) is received from the agent,
            // iterate over all connected web clients and send them the message.
            for (const client of webClients) {
                if (client.readyState === WebSocket.OPEN) {
                    client.send(message); // Forward the raw message directly
                }
            }
        });

        ws.on('close', () => {
            console.log('Client (Agent) disconnected');
            agentWs = null; // Clear agent reference on disconnect
        });

        ws.on('error', error => {
            console.error('WebSocket error with Agent:', error);
            agentWs = null;
        });

    } else {
        // This is likely a web browser client
        webClients.add(ws);
        console.log('Web client connected');

        ws.on('close', () => {
            console.log('Web client disconnected');
            webClients.delete(ws); // Remove from the set on disconnect
        });

        ws.on('error', error => {
            console.error('WebSocket error with Web Client:', error);
            webClients.delete(ws);
        });

        // Web clients don't send frames to the server in this architecture,
        // but if they did, you'd handle messages here.
        ws.on('message', message => {
            console.log('Received message from web client:', message.toString());
            // You could add logic here to send commands back to the agent if needed
        });
    }
});

server.listen(PORT, () => {
    console.log(`HTTP and WebSocket server listening on port ${PORT}`);
    console.log(`Access web client at http://localhost:${PORT}/index.html`);
});