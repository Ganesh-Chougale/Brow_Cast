`Remote-Share\Server`  
### 1. Initialize Node.js project  
```bash
npm init -y
```  

### 2. Install dependencies  
```bash
npm install express ws
```  

### 3. Create `index.js`  
```bash
touch index.js
# because in package.json file index.js is main entry point of application
```  
```javascript
// server/server.js
const express = require('express');
const WebSocket = require('ws');
const http = require('http');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

wss.on('connection', ws => {
    console.log('Client connected');

    ws.on('message', message => {
        console.log(`Received: ${message}`);
        // In later phases, you'll add logic to relay messages between agent and web client
        ws.send(`Echo: ${message}`); // Simple echo for now
    });

    ws.on('close', () => {
        console.log('Client disconnected');
    });

    ws.on('error', error => {
        console.error('WebSocket error:', error);
    });
});

// Serve static files from the 'web' directory in a later phase
// app.use(express.static('../web'));

const PORT = process.env.PORT || 8080;
server.listen(PORT, () => {
    console.log(`Signaling server listening on port ${PORT}`);
});
```  