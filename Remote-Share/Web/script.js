let connectButton;
let sessionIdInput;
let statusMessage;
let remoteScreenCanvas;
let loadingOverlay;
let controlButtons;
let fullScreenButton;
let disconnectButton;
let ctx;
let isFullScreen = false;
let ws = null;
let originalWidth = 0; // Stores the original width of the remote screen frame
let originalHeight = 0; // Stores the original height of the remote screen frame
let ipDialog;
let ipInputInDialog;
let ipDialogConnectButton;
let ipDialogStatusMessage;

// Function to show the IP input dialog
function showIpInputDialog(callback) {
    if (!ipDialog) {
        ipDialog = document.createElement('div');
        ipDialog.id = 'ipDialog';
        // Bootstrap modal-like styling
        ipDialog.className = 'modal fade show d-block'; // Use d-block to show immediately, fade for animation
        ipDialog.setAttribute('tabindex', '-1');
        ipDialog.setAttribute('aria-labelledby', 'ipDialogLabel');
        ipDialog.setAttribute('aria-hidden', 'true');
        ipDialog.style.backgroundColor = 'rgba(0,0,0,0.75)'; // Custom backdrop color

        ipDialog.innerHTML = `
            <div class="modal-dialog modal-dialog-centered">
                <div class="modal-content">
                    <div class="modal-header">
                        <h5 class="modal-title" id="ipDialogLabel">Enter Server IP Address</h5>
                    </div>
                    <div class="modal-body">
                        <p class="text-muted small mb-3">
                            The viewer could not connect to 'localhost'. Please enter the IP address of the machine running the Node.js server.
                            (Check the server's console output for the IP address).
                        </p>
                        <input type="text" id="ipInputInDialog" placeholder="e.g., 192.168.1.100" 
                               class="form-control mb-3">
                        <div id="ipDialogStatusMessage" class="text-danger small mb-3"></div>
                    </div>
                    <div class="modal-footer">
                        <button id="ipDialogConnectButton" class="btn btn-primary w-100">
                            Connect to IP
                        </button>
                    </div>
                </div>
            </div>
        `;
        document.body.appendChild(ipDialog);

        ipInputInDialog = document.getElementById('ipInputInDialog');
        ipDialogConnectButton = document.getElementById('ipDialogConnectButton');
        ipDialogStatusMessage = document.getElementById('ipDialogStatusMessage');

        ipDialogConnectButton.addEventListener('click', () => {
            const ip = ipInputInDialog.value.trim();
            if (ip) {
                ipDialogStatusMessage.textContent = '';
                callback(ip);
                ipDialog.classList.remove('d-block'); // Hide the dialog
                ipDialog.classList.add('d-none');
            } else {
                ipDialogStatusMessage.textContent = 'IP address cannot be empty.';
            }
        });

        ipInputInDialog.addEventListener('keypress', (event) => {
            if (event.key === 'Enter') {
                ipDialogConnectButton.click();
            }
        });
    } else {
        ipDialogStatusMessage.textContent = '';
        ipDialog.classList.remove('d-none');
        ipDialog.classList.add('d-block');
    }
    ipInputInDialog.focus();
}

// Function to update status messages
function updateStatus(message, type = 'info') {
    if (statusMessage) {
        statusMessage.textContent = message;
        // Remove previous Bootstrap text color classes
        statusMessage.classList.remove('text-success', 'text-danger', 'text-warning', 'text-muted');

        // Add new Bootstrap text color class based on type
        if (type === 'success') {
            statusMessage.classList.add('text-success');
        } else if (type === 'error') {
            statusMessage.classList.add('text-danger');
        } else if (type === 'warning') {
            statusMessage.classList.add('text-warning');
        } else {
            statusMessage.classList.add('text-muted'); // Default for 'info'
        }
    }
}

// Function to connect to WebSocket
function connectWebSocket(serverIp = 'localhost') {
    const sessionId = sessionIdInput.value.trim();
    if (!sessionId) {
        updateStatus('Please enter a session ID.', 'warning');
        return;
    }

    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
    }

    // Show loading overlay using Bootstrap d-none/d-block
    if (loadingOverlay) {
        loadingOverlay.classList.remove('d-none');
    }

    updateStatus('Connecting...', 'info');
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${serverIp}:8080/viewer?sessionId=${sessionId}`; // Ensure port 8080 or your server's port

    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        updateStatus('Connected!', 'success');
        if (loadingOverlay) {
            loadingOverlay.classList.add('d-none');
        }
        if (controlButtons) {
            controlButtons.classList.remove('d-none');
        }
        console.log('WebSocket connected.');
        ws.send(JSON.stringify({ type: 'viewer_ready', sessionId: sessionId }));
    };

    ws.onmessage = (event) => {
        try {
            const message = JSON.parse(event.data);
            if (message.type === 'frame' && message.image) {
                const img = new Image();
                img.src = message.image;
                img.onload = () => {
                    if (!ctx || !remoteScreenCanvas) {
                        console.error('Canvas context or element not available for drawing.');
                        return;
                    }

                    // Store original dimensions for input scaling
                    originalWidth = message.width;
                    originalHeight = message.height;

                    // Set the canvas's internal drawing buffer resolution to match the image
                    // This ensures high-quality drawing and correct aspect ratio for CSS scaling
                    remoteScreenCanvas.width = img.width;
                    remoteScreenCanvas.height = img.height;

                    // Clear the canvas before drawing the new frame
                    ctx.clearRect(0, 0, remoteScreenCanvas.width, remoteScreenCanvas.height);
                    
                    // Draw the image directly onto the canvas.
                    // The CSS (width: 100%; height: auto;) will handle the display scaling
                    // of this high-resolution drawing buffer to fit the parent container.
                    ctx.drawImage(img, 0, 0, img.width, img.height);
                };
            } else if (message.type === 'agent_status') {
                if (message.connected) {
                    updateStatus('Agent Connected', 'success');
                } else {
                    updateStatus('Agent Disconnected', 'warning');
                }
            } else {
                console.log('Received unknown message from server:', message);
            }
        } catch (e) {
            console.error('Error parsing or handling WebSocket message:', e);
            updateStatus('Error receiving data.', 'danger'); // Changed to 'danger' for error type
        }
    };

    ws.onerror = (error) => {
        console.error('WebSocket Error:', error);
        updateStatus('Connection Error. Check server status or session ID.', 'danger');
        if (loadingOverlay) {
            loadingOverlay.classList.add('d-none');
        }
        // If connection error, show IP dialog if it was default 'localhost'
        if (serverIp === 'localhost') {
            showIpInputDialog(connectWebSocket);
        }
    };

    ws.onclose = () => {
        closeConnection();
    };
}

// Function to send input events (mouse, keyboard) to the agent
function sendInput(inputType, data) {
    if (ws && ws.readyState === WebSocket.OPEN && originalWidth > 0 && originalHeight > 0 && remoteScreenCanvas) {
        let scaledData = { ...data };
        if (inputType.startsWith('mouse') || inputType === 'click' || inputType === 'contextmenu' || inputType === 'wheel') {
            if (data.x !== undefined && data.y !== undefined) {
                const rect = remoteScreenCanvas.getBoundingClientRect(); // Get canvas's current displayed size
                const canvasAspectRatio = remoteScreenCanvas.width / remoteScreenCanvas.height; // Internal drawing buffer aspect ratio
                const imageAspectRatio = originalWidth / originalHeight; // Original captured image aspect ratio

                // The canvas is now always drawn at originalWidth/Height, and CSS scales it.
                // We need to scale clientX/Y back to the original resolution based on the *displayed* canvas size.
                // This logic is mostly correct from your original code, as it accounts for letterboxing
                // if the canvas's *displayed* aspect ratio doesn't match the image's aspect ratio.

                let displayedImageWidth, displayedImageHeight;
                let displayOffsetX = 0, displayOffsetY = 0;

                // Calculate the dimensions of the *image as it's displayed within the canvas element's bounds*
                // This logic correctly handles the "aspect-fit" that CSS now implicitly does for the canvas.
                if (imageAspectRatio > (rect.width / rect.height)) { // If image is wider than actual display area
                    displayedImageWidth = rect.width;
                    displayedImageHeight = rect.width / imageAspectRatio;
                    displayOffsetY = (rect.height - displayedImageHeight) / 2;
                } else { // If image is taller than actual display area
                    displayedImageHeight = rect.height;
                    displayedImageWidth = rect.height * imageAspectRatio;
                    displayOffsetX = (rect.width - displayedImageWidth) / 2;
                }
                
                // Calculate click position relative to the *displayed image* within the canvas bounds
                const clickXRelativeToImage = data.x - rect.left - displayOffsetX;
                const clickYRelativeToImage = data.y - rect.top - displayOffsetY;

                // Scale these relative coordinates back to the original screen resolution
                const scaleX = originalWidth / displayedImageWidth;
                const scaleY = originalHeight / displayedImageHeight;

                scaledData.x = Math.round(clickXRelativeToImage * scaleX);
                scaledData.y = Math.round(clickYRelativeToImage * scaleY);

                // Clamp coordinates to prevent sending out-of-bounds values
                scaledData.x = Math.max(0, Math.min(scaledData.x, originalWidth - 1));
                scaledData.y = Math.max(0, Math.min(scaledData.y, originalHeight - 1));
            }
        }
        const message = {
            type: 'input',
            inputType: inputType,
            ...scaledData
        };
        ws.send(JSON.stringify(message));
    }
}

// Function to toggle full screen mode
function toggleFullScreen() {
    try {
        if (!document.fullscreenElement) {
            if (remoteScreenCanvas.requestFullscreen) {
                remoteScreenCanvas.requestFullscreen();
            } else if (remoteScreenCanvas.webkitRequestFullscreen) { // Safari
                remoteScreenCanvas.webkitRequestFullscreen();
            } else if (remoteScreenCanvas.msRequestFullscreen) { // IE/Edge
                remoteScreenCanvas.msRequestFullscreen();
            }
            isFullScreen = true;
            fullScreenButton.textContent = 'Exit Full Screen';
        } else {
            if (document.exitFullscreen) {
                document.exitFullscreen();
            } else if (document.webkitExitFullscreen) { // Safari
                document.webkitExitFullscreen();
            } else if (document.msExitFullscreen) { // IE/Edge
                document.msExitFullscreen();
            }
            isFullScreen = false;
            fullScreenButton.textContent = 'Full Screen';
        }
    } catch (e) {
        console.error('Error toggling fullscreen:', e);
    }
}

// Function to close WebSocket connection
function closeConnection() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
    }
    if (controlButtons) {
        controlButtons.classList.add('d-none'); // Hide with Bootstrap class
    }
    if (loadingOverlay) {
        loadingOverlay.classList.add('d-none'); // Hide with Bootstrap class
    }
    updateStatus('Disconnected', 'info');

    // Clear canvas and draw 'Disconnected' message
    if (ctx && remoteScreenCanvas) {
        // Reset canvas to a default initial size for the disconnected state
        remoteScreenCanvas.width = 800; // Example default size
        remoteScreenCanvas.height = 600; // Example default size
        
        ctx.clearRect(0, 0, remoteScreenCanvas.width, remoteScreenCanvas.height);
        ctx.fillStyle = '#000000'; // Black background
        ctx.fillRect(0, 0, remoteScreenCanvas.width, remoteScreenCanvas.height);
        ctx.fillStyle = '#ffffff'; // White text
        ctx.font = '16px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('Disconnected', remoteScreenCanvas.width / 2, remoteScreenCanvas.height / 2);
    }
}

// DOMContentLoaded event listener
document.addEventListener('DOMContentLoaded', () => {
    // Get DOM elements
    connectButton = document.getElementById('connectButton');
    sessionIdInput = document.getElementById('sessionIdInput');
    statusMessage = document.getElementById('statusMessage');
    remoteScreenCanvas = document.getElementById('remoteScreenCanvas');
    loadingOverlay = document.getElementById('loadingOverlay');
    controlButtons = document.getElementById('controlButtons');
    fullScreenButton = document.getElementById('fullScreenButton');
    disconnectButton = document.getElementById('disconnectButton');

    // Initialize canvas
    if (remoteScreenCanvas) {
        ctx = remoteScreenCanvas.getContext('2d');
        // Set initial canvas drawing buffer size for the 'Waiting for connection...' message
        remoteScreenCanvas.width = 800; 
        remoteScreenCanvas.height = 600;
        ctx.fillStyle = '#000000';
        ctx.fillRect(0, 0, remoteScreenCanvas.width, remoteScreenCanvas.height);
        ctx.fillStyle = '#ffffff';
        ctx.font = '16px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('Waiting for connection...', remoteScreenCanvas.width / 2, remoteScreenCanvas.height / 2);
    }

    // Event Listeners
    if (fullScreenButton) {
        fullScreenButton.addEventListener('click', toggleFullScreen);
    }
    if (disconnectButton) {
        disconnectButton.addEventListener('click', closeConnection);
    }

    // Keyboard events for input and fullscreen
    document.addEventListener('keydown', (e) => {
        if (e.key === 'F11') {
            e.preventDefault(); // Prevent default F11 browser behavior
            toggleFullScreen();
        }
        // Check if Escape key is pressed AND we are in full screen (browser handles exiting fullscreen on Escape, but good to have explicit logic)
        // Note: Browser usually exits fullscreen automatically on Escape. This is more for updating UI state.
        if (e.key === 'Escape' && isFullScreen) {
            // toggleFullScreen(); // Calling this might cause double-toggling if browser also acts
            // Best to let fullscreenchange event update isFullScreen and button text.
        }
        sendInput('keydown', {
            key: e.key,
            code: e.code,
            keyCode: e.keyCode,
            ctrlKey: e.ctrlKey,
            shiftKey: e.shiftKey,
            altKey: e.altKey,
            metaKey: e.metaKey
        });
    });

    document.addEventListener('keyup', (e) => {
        sendInput('keyup', {
            key: e.key,
            code: e.code,
            keyCode: e.keyCode,
            ctrlKey: e.ctrlKey,
            shiftKey: e.shiftKey,
            altKey: e.altKey,
            metaKey: e.metaKey
        });
    });

    // Fullscreen change events
    document.addEventListener('fullscreenchange', () => {
        isFullScreen = !!document.fullscreenElement;
        fullScreenButton.textContent = isFullScreen ? 'Exit Full Screen' : 'Full Screen';
    });
    document.addEventListener('webkitfullscreenchange', () => {
        isFullScreen = !!document.webkitFullscreenElement;
        fullScreenButton.textContent = isFullScreen ? 'Exit Full Screen' : 'Full Screen';
    });
    document.addEventListener('msfullscreenchange', () => {
        isFullScreen = !!document.msFullscreenElement;
        fullScreenButton.textContent = isFullScreen ? 'Exit Full Screen' : 'Full Screen';
    });

    // Connect button and session ID input listeners
    if (connectButton) {
        // Initial connection attempt to localhost, if it fails, the dialog will appear
        connectButton.addEventListener('click', () => connectWebSocket('localhost'));
    }
    if (sessionIdInput) {
        sessionIdInput.addEventListener('keypress', (event) => {
            if (event.key === 'Enter') {
                connectWebSocket('localhost');
            }
        });
    }

    // Mouse input listeners for canvas
    if (remoteScreenCanvas) {
        remoteScreenCanvas.addEventListener('mousemove', (e) => sendInput('mousemove', { x: e.clientX, y: e.clientY }));
        remoteScreenCanvas.addEventListener('mousedown', (e) => sendInput('mousedown', { x: e.clientX, y: e.clientY, button: e.button }));
        remoteScreenCanvas.addEventListener('mouseup', (e) => sendInput('mouseup', { x: e.clientX, y: e.clientY, button: e.button }));
        remoteScreenCanvas.addEventListener('click', (e) => sendInput('click', { x: e.clientX, y: e.clientY, button: e.button }));
        
        remoteScreenCanvas.addEventListener('contextmenu', (e) => {
            e.preventDefault(); // Prevent browser's context menu
            sendInput('contextmenu', { x: e.clientX, y: e.clientY, button: e.button });
        });

        remoteScreenCanvas.addEventListener('wheel', (e) => {
            e.preventDefault(); // Prevent page scrolling
            sendInput('wheel', { deltaY: e.deltaY, x: e.clientX, y: e.clientY });
        });
    }
});