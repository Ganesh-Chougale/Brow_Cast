`stream-server.js`  
```javascript
// stream-server.js
const express = require('express');
const { spawn } = require('child_process');
const cors = require('cors'); // Required for cross-origin requests from frontend

const app = express();
const port = 3000;

app.use(cors()); // Enable CORS for all routes

// Serve static files from a 'public' directory (for your frontend HTML)
app.use(express.static('public'));

app.get('/stream', (req, res) => {
    // Set response headers for MJPEG stream
    res.writeHead(200, {
        'Content-Type': 'multipart/x-mixed-replace; boundary=--boundaryframer',
        'Cache-Control': 'no-cache',
        'Connection': 'keep-alive',
    });

    // Start FFmpeg process
    const ffmpeg = spawn('ffmpeg', [
        '-f', 'gdigrab',
        '-i', 'title=Notepad', // Make sure Notepad is open
        '-f', 'mjpeg',
        '-q:v', '5', // Video quality (0-31, lower is better)
        '-r', '15', // Frame rate
        '-' // Output to stdout
    ]);

    console.log('FFmpeg process spawned.');

    // Pipe FFmpeg's stdout (MJPEG frames) directly to the HTTP response
    ffmpeg.stdout.on('data', (data) => {
        // Each 'data' chunk from FFmpeg stdout is an MJPEG frame
        res.write('--boundaryframer\r\n');
        res.write('Content-Type: image/jpeg\r\n');
        res.write('Content-Length: ' + data.length + '\r\n');
        res.write('\r\n');
        res.write(data);
        res.write('\r\n');
    });

    ffmpeg.stderr.on('data', (data) => {
        // FFmpeg often outputs progress and error messages to stderr
        console.error(`FFmpeg stderr: ${data}`);
    });

    ffmpeg.on('close', (code) => {
        console.log(`FFmpeg process exited with code ${code}`);
        res.end(); // End the response if FFmpeg closes
    });

    ffmpeg.on('error', (err) => {
        console.error('Failed to start FFmpeg process:', err);
        res.status(500).send('Failed to start streaming service.');
    });

    // Handle client disconnect (browser closes tab, navigates away)
    req.on('close', () => {
        console.log('Client disconnected. Killing FFmpeg process...');
        ffmpeg.kill('SIGINT'); // Send interrupt signal to FFmpeg
    });
});

app.listen(port, () => {
    console.log(`Stream server listening at http://localhost:${port}`);
    console.log(`Open http://localhost:${port}/ to view the stream.`);
    console.log(`Make sure 'Notepad' is running on your system.`);
});
```  
```bash
ffmpeg -f gdigrab -i title="Untitled - Notepad" -f mjpeg -q:v 5 -r 15 - | node stream-server.js
```  