1. Remote-Share/Agent/build (main)
$ ./RemoteShareAgent.exe
```terminal
Waiting for WebSocket connection to establish...
WebSocket connected to server.
WebSocket disconnected from server. Attempting reconnect...
WebSocket client run loop ended. Attempting to reconnect...
Failed to connect to WebSocket server within timeout. Ensure Node.js server is running on ws://localhost:8080/agent
```

2. Remote-Share/Server (main)
$ node index.js 
```terminal
$ node index.js 
Server running on http://localhost:8080
Web client available at http://localhost:8080/index.html
Listening on port 8080
Connection attempt without sessionId. Closing connection.
```

3. Browser
Cannot GET /index.html
$ node index.js 
```Console
index.html:1 
GET http://localhost:8080/index.html 404 (Not Found)
content_script_bundle.js:1 Attempting initialization Wed Jun 25 2025 22:09:41 GMT+0530 (India Standard Time)
```