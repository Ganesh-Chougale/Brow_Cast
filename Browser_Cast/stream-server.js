const express = require('express');
const { spawn } = require('child_process');
const cors = require('cors');

const appname = "Calculator"; // Or any other application name you want to stream
const apptitle = `title=${appname}`; // CORRECTED LINE: No extra single quotes

const app = express();
const port = 3000;

app.use(cors());

app.use(express.static('public'));

app.get('/stream', (req, res) => {
    res.writeHead(200, {
        'Content-Type': 'multipart/x-mixed-replace; boundary=--boundaryframer',
        'Cache-Control': 'no-cache',
        'Connection': 'keep-alive',
    });

    // Start FFmpeg process
    const ffmpeg = spawn('ffmpeg', [
        '-f', 'gdigrab',
        '-i', apptitle, // This now passes "title=Calculator" correctly
        '-f', 'mjpeg',
        '-q:v', '5',
        '-r', '30',
        '-'
    ]);

    console.log('FFmpeg process spawned.');

    ffmpeg.stdout.on('data', (data) => {
        res.write('--boundaryframer\r\n');
        res.write('Content-Type: image/jpeg\r\n');
        res.write('Content-Length: ' + data.length + '\r\n');
        res.write('\r\n');
        res.write(data);
        res.write('\r\n');
    });

    ffmpeg.stderr.on('data', (data) => {
        console.error(`FFmpeg stderr: ${data.toString()}`);
    });

    ffmpeg.on('close', (code) => {
        console.log(`FFmpeg process exited with code ${code}`);
        res.end();
    });

    ffmpeg.on('error', (err) => {
        console.error('Failed to start FFmpeg process:', err);
        res.status(500).send('Failed to start streaming service.');
    });

    req.on('close', () => {
        console.log('Client disconnected. Killing FFmpeg process...');
        if (!ffmpeg.killed) {
            ffmpeg.kill('SIGINT');
        }
    });
});

app.listen(port, () => {
    console.log(`Stream server listening at http://localhost:${port}`);
    console.log(`Open http://localhost:${port}/ to view the stream.`);
    console.log(`Make sure '${appname}' is running on your system.`); // Use appname directly
});