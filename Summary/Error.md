1. Remote-Share/Agent/build (main)
$ ./RemoteShareAgent.exe
```terminal
$ ./RemoteShareAgent.exe 
Waiting for WebSocket connection to establish...
WebSocket connected to server.
WebSocket disconnected from server. Attempting reconnect (not implemented yet)...
WebSocket client run loop ended. Attempting to reconnect...
Failed to connect to WebSocket server within timeout. Ensure Node.js server is running on ws://localhost:8080
```

2. Remote-Share/Server (main)
$ node index.js 
```terminal
$ node index.js 
Server running on http://localhost:8080
Web client available at http://localhost:8080/index.html
Unknown connection attempt from: /. Closing.
Unknown connection attempt from: /. Closing.
Unknown connection attempt from: /. Closing.
Unknown connection attempt from: /. Closing.
Unknown connection attempt from: /. Closing.
Unknown connection attempt from: /. Closing.
Unknown connection attempt from: /. Closing.
Unknown connection attempt from: /. Closing
```

3. Browser
blank white screen
$ node index.js 
```Console
script.js:129 Uncaught TypeError: Cannot set properties of undefined (setting 'onopen')
    at script.js:129:11
(anonymous) @ script.js:129
script.js:29 WebSocket connection established with server.
content_script_bundle.js:1 Attempting initialization Wed Jun 25 2025 20:47:57 GMT+0530 (India Standard Time)
script.js:60 WebSocket connection closed. Attempting to reconnect...
script.js:29 WebSocket connection established with server.
```