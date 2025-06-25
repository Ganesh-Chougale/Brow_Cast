    const canvas = document.getElementById('remoteCanvas');
    const ctx = canvas.getContext('2d');

    function resizeCanvas() {
        canvas.width = window.innerWidth;
        canvas.height = window.innerHeight;
    }

    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);

    let ws; // Declare ws here, will be assigned in connectWebSocket()

    // Create and append the status overlay element
    const statusOverlay = document.createElement('div');
    statusOverlay.style.position = 'fixed';
    statusOverlay.style.bottom = '10px';
    statusOverlay.style.right = '10px';
    statusOverlay.style.padding = '10px';
    statusOverlay.style.backgroundColor = 'rgba(0, 0, 0, 0.7)';
    statusOverlay.style.color = 'white';
    statusOverlay.style.borderRadius = '5px';
    statusOverlay.style.zIndex = '1000';
    statusOverlay.textContent = 'Connecting...';
    document.body.appendChild(statusOverlay);

    // Function to update the text content of the status overlay
    function updateStatus(status) {
        statusOverlay.textContent = status;
    }

    function connectWebSocket() {
        updateStatus('Connecting...'); // Set initial status when trying to connect
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        // The web client now connects to the /viewer path on the WebSocket server.
        const wsUrl = protocol + window.location.host + '/viewer'; 

        ws = new WebSocket(wsUrl); // Initialize the WebSocket object

        // Set up WebSocket event handlers AFTER the ws object is created
        ws.onopen = () => {
            console.log('WebSocket connection established with server.');
            updateStatus('Connected');
            // Fade out the status message after a delay
            setTimeout(() => {
                statusOverlay.style.transition = 'opacity 1s ease-out'; // Smooth transition
                statusOverlay.style.opacity = '0'; // Make it disappear
            }, 2000);
        };

        ws.onmessage = (event) => {
            try {
                let message;
                // Handle binary data (Blob) or text data from WebSocket
                if (event.data instanceof Blob) {
                    const reader = new FileReader();
                    reader.onload = function() {
                        try {
                            message = JSON.parse(reader.result);
                            handleFrameMessage(message);
                        } catch (e) {
                            console.error('Error parsing Blob message:', e);
                        }
                    };
                    reader.readAsText(event.data);
                    return; // Exit as processing is asynchronous
                } else {
                    message = JSON.parse(event.data);
                }
                handleFrameMessage(message);
            } catch (e) {
                console.error('Error processing message:', e);
            }
        };

        ws.onclose = () => {
            console.log('WebSocket connection closed. Attempting to reconnect...');
            updateStatus('Disconnected. Reconnecting...');
            statusOverlay.style.opacity = '1'; // Make status visible again
            setTimeout(connectWebSocket, 3000); // Attempt to reconnect after 3 seconds
        };

        ws.onerror = (error) => {
            console.error('WebSocket error:', error);
            // The onclose handler will typically be called after onerror,
            // so direct ws.close() call is often not needed here unless
            // specific error recovery logic is required.
        };
    }

    // Handles incoming messages, specifically frame data and agent status updates
    function handleFrameMessage(message) {
        if (message.type === 'frame' && message.image) {
            const img = new Image();
            img.onload = () => {
                ctx.clearRect(0, 0, canvas.width, canvas.height); // Clear previous frame

                // Calculate aspect ratios for fitting the image to the canvas
                const canvasAspect = canvas.width / canvas.height;
                const imgAspect = img.width / img.height;

                let drawWidth, drawHeight, offsetX = 0, offsetY = 0;

                if (imgAspect > canvasAspect) {
                    // Image is wider relative to its height than the canvas
                    // Fit by canvas height, calculate width
                    drawHeight = canvas.height;
                    drawWidth = drawHeight * imgAspect;
                    offsetX = (canvas.width - drawWidth) / 2; // Center horizontally
                } else {
                    // Image is taller relative to its width than the canvas
                    // Fit by canvas width, calculate height
                    drawWidth = canvas.width;
                    drawHeight = drawWidth / imgAspect;
                    offsetY = (canvas.height - drawHeight) / 2; // Center vertically
                }
                ctx.drawImage(img, offsetX, offsetY, drawWidth, drawHeight); // Draw the image
            };
            img.src = 'data:image/jpeg;base64,' + message.image; // Set image source from base64 data
        } else if (message.type === 'agent_status') {
            // Update status overlay based on agent connection status
            if (message.connected) {
                updateStatus('Agent Connected');
                setTimeout(() => {
                    statusOverlay.style.transition = 'opacity 1s ease-out';
                    statusOverlay.style.opacity = '0';
                }, 2000);
            } else {
                updateStatus('Agent Disconnected');
                statusOverlay.style.opacity = '1'; // Ensure status is visible when disconnected
            }
        } else {
            console.log('Received message from server:', message);
        }
    }

    // Initiate the WebSocket connection when the entire DOM is loaded
    window.addEventListener('DOMContentLoaded', connectWebSocket);
    