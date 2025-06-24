// Remote-Share/Server/public/script.js

const ws = new WebSocket('ws://localhost:8080'); // Your WebSocket URL

ws.onopen = () => {
    console.log('WebSocket connection established with server.');
};

ws.onmessage = (event) => {
    // console.log("Raw message data type:", typeof event.data, event.data); // For debugging: check what you receive

    // If the data is a Blob, convert it to text first
    if (event.data instanceof Blob) {
        const reader = new FileReader();
        reader.onload = function() {
            try {
                const message = JSON.parse(reader.result);
                handleFrameMessage(message); // Call a function to process the frame
            } catch (e) {
                console.error('Error parsing or processing WebSocket message (from Blob):', e);
            }
        };
        reader.readAsText(event.data);
    } else if (typeof event.data === 'string') {
        // If it's already a string (expected JSON)
        try {
            const message = JSON.parse(event.data);
            handleFrameMessage(message); // Call a function to process the frame
        } catch (e) {
            console.error('Error parsing or processing WebSocket message (from string):', e);
        }
    } else {
        console.error("Unknown message type received:", typeof event.data, event.data);
    }
};

// --- Add this new function to handle the frame message ---
function handleFrameMessage(message) {
    if (message.type === 'frame' && message.image) {
        const img = document.getElementById('screenShareImage');
        if (img) {
            img.src = 'data:image/jpeg;base64,' + message.image;
            // Optionally, update dimensions if needed (your HTML should handle this with max-width/height CSS)
            // img.style.width = message.width + 'px';
            // img.style.height = message.height + 'px';
        }
    } else {
        console.log('Received message from server:', message);
    }
}

ws.onclose = () => {
    console.log('WebSocket connection closed.');
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
};

// Ensure your HTML has an image tag with id="screenShareImage"
// <img id="screenShareImage" style="max-width: 100%; height: auto;" />