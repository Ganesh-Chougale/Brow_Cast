// Global variables to be initialized after DOM is ready
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

// WebSocket Variables
let ws = null;
let originalWidth = 0; // Stores the original width of the remote screen/window from agent
let originalHeight = 0; // Stores the original height of the remote screen/window from agent

// Dialog elements for IP input
let ipDialog;
let ipInputInDialog;
let ipDialogConnectButton;
let ipDialogStatusMessage;

/**
 * Shows a modal dialog to get the server IP from the user.
 * @param {function} callback - Function to call with the entered IP.
 */
function showIpInputDialog(callback) {
    if (!ipDialog) {
        // Create dialog elements if they don't exist
        ipDialog = document.createElement('div');
        ipDialog.id = 'ipDialog';
        ipDialog.className = 'fixed inset-0 bg-gray-900 bg-opacity-75 flex items-center justify-center z-50';
        ipDialog.innerHTML = `
            <div class="bg-white p-6 rounded-lg shadow-xl text-center">
                <h3 class="text-xl font-bold mb-4 text-gray-800">Enter Server IP Address</h3>
                <p class="text-sm text-gray-600 mb-4">
                    The viewer could not connect to 'localhost'. Please enter the IP address of the machine running the Node.js server.
                    (Check the server's console output for the IP address).
                </p>
                <input type="text" id="ipInputInDialog" placeholder="e.g., 192.168.1.100" 
                       class="p-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500 w-full mb-3">
                <div id="ipDialogStatusMessage" class="text-sm text-red-600 mb-3"></div>
                <button id="ipDialogConnectButton" 
                        class="bg-blue-600 hover:bg-blue-700 text-white font-semibold py-2 px-4 rounded-md shadow-md transition duration-300 ease-in-out w-full">
                    Connect to IP
                </button>
            </div>
        `;
        document.body.appendChild(ipDialog);

        // Get references to dialog elements
        ipInputInDialog = document.getElementById('ipInputInDialog');
        ipDialogConnectButton = document.getElementById('ipDialogConnectButton');
        ipDialogStatusMessage = document.getElementById('ipDialogStatusMessage');

        // Add event listener for connect button in dialog
        ipDialogConnectButton.addEventListener('click', () => {
            const ip = ipInputInDialog.value.trim();
            if (ip) {
                ipDialogStatusMessage.textContent = '';
                callback(ip);
                ipDialog.classList.add('hidden'); // Hide dialog on successful input
            } else {
                ipDialogStatusMessage.textContent = 'IP address cannot be empty.';
            }
        });

        // Allow pressing Enter in the dialog input field
        ipInputInDialog.addEventListener('keypress', (event) => {
            if (event.key === 'Enter') {
                ipDialogConnectButton.click();
            }
        });
    } else {
        // Just show it if already created
        ipDialogStatusMessage.textContent = ''; // Clear previous status
        ipDialog.classList.remove('hidden');
    }
    ipInputInDialog.focus();
}

/**
 * Updates the connection status message displayed on the UI.
 * @param {string} message - The message to display.
 * @param {string} type - 'info', 'success', 'error', 'warning' (for styling).
 */
function updateStatus(message, type = 'info') {
    if (statusMessage) {
        statusMessage.textContent = message;
        statusMessage.className = 'text-center text-sm font-medium mb-4';
        if (type === 'success') {
            statusMessage.classList.add('text-green-600');
        } else if (type === 'error') {
            statusMessage.classList.add('text-red-600');
        } else if (type === 'warning') {
            statusMessage.classList.add('text-orange-600');
        } else {
            statusMessage.classList.add('text-gray-600');
        }
    }
}

/**
 * Initiates the WebSocket connection to the server.
 * @param {string} serverIp - The IP address of the server. Defaults to 'localhost'.
 */
function connectWebSocket(serverIp = 'localhost') {
    const sessionId = sessionIdInput.value.trim();
    if (!sessionId) {
        updateStatus('Please enter a session ID.', 'warning');
        return;
    }

    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
    }

    if (loadingOverlay) {
        loadingOverlay.classList.remove('hidden');
    }
    updateStatus('Connecting...', 'info');

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${serverIp}:8080/viewer?sessionId=${sessionId}`;

    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        updateStatus('Connected!', 'success');
        if (loadingOverlay) {
            loadingOverlay.classList.add('hidden');
        }
        if (controlButtons) {
            controlButtons.classList.remove('hidden');
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

                    originalWidth = message.width;
                    originalHeight = message.height;

                    remoteScreenCanvas.width = originalWidth;
                    remoteScreenCanvas.height = originalHeight;

                    const canvasAspectRatio = remoteScreenCanvas.width / remoteScreenCanvas.height;
                    const imageAspectRatio = img.width / img.height;

                    let drawWidth, drawHeight, offsetX = 0, offsetY = 0;

                    if (imageAspectRatio > canvasAspectRatio) {
                        drawWidth = remoteScreenCanvas.width;
                        drawHeight = remoteScreenCanvas.width / imageAspectRatio;
                        offsetY = (remoteScreenCanvas.height - drawHeight) / 2;
                    } else {
                        drawHeight = remoteScreenCanvas.height;
                        drawWidth = remoteScreenCanvas.height * imageAspectRatio;
                        offsetX = (remoteScreenCanvas.width - drawWidth) / 2;
                    }

                    ctx.clearRect(0, 0, remoteScreenCanvas.width, remoteScreenCanvas.height);
                    ctx.drawImage(img, offsetX, offsetY, drawWidth, drawHeight);
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
            updateStatus('Error receiving data.', 'error');
        }
    };

    ws.onerror = (error) => {
        console.error('WebSocket Error:', error);
        updateStatus('Connection Error. Check server status or session ID.', 'error');
        if (loadingOverlay) {
            loadingOverlay.classList.add('hidden');
        }
    };

    ws.onclose = () => {
        closeConnection();
    };
}

/**
 * Sends an input event (mouse or keyboard) to the server via WebSocket.
 */
function sendInput(inputType, data) {
    if (ws && ws.readyState === WebSocket.OPEN && originalWidth > 0 && originalHeight > 0 && remoteScreenCanvas) {
        let scaledData = { ...data };

        if (inputType.startsWith('mouse') || inputType === 'click' || inputType === 'contextmenu' || inputType === 'wheel') {
            if (data.x !== undefined && data.y !== undefined) {
                const rect = remoteScreenCanvas.getBoundingClientRect();
                
                const canvasAspectRatio = remoteScreenCanvas.width / remoteScreenCanvas.height;
                const imageAspectRatio = originalWidth / originalHeight;

                let displayedImageWidth, displayedImageHeight;
                let displayOffsetX = 0, displayOffsetY = 0;

                if (imageAspectRatio > canvasAspectRatio) {
                    displayedImageWidth = rect.width;
                    displayedImageHeight = rect.width / imageAspectRatio;
                    displayOffsetY = (rect.height - displayedImageHeight) / 2;
                } else {
                    displayedImageHeight = rect.height;
                    displayedImageWidth = rect.height * imageAspectRatio;
                    displayOffsetX = (rect.width - displayedImageWidth) / 2;
                }

                const clickXRelativeToImage = data.x - rect.left - displayOffsetX;
                const clickYRelativeToImage = data.y - rect.top - displayOffsetY;

                const scaleX = originalWidth / displayedImageWidth;
                const scaleY = originalHeight / displayedImageHeight;

                scaledData.x = Math.round(clickXRelativeToImage * scaleX);
                scaledData.y = Math.round(clickYRelativeToImage * scaleY);

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

/**
 * Toggle fullscreen mode for the canvas
 */
function toggleFullScreen() {
    try {
        if (!document.fullscreenElement) {
            // Enter fullscreen
            if (remoteScreenCanvas.requestFullscreen) {
                remoteScreenCanvas.requestFullscreen();
            } else if (remoteScreenCanvas.webkitRequestFullscreen) { /* Safari */
                remoteScreenCanvas.webkitRequestFullscreen();
            } else if (remoteScreenCanvas.msRequestFullscreen) { /* IE11 */
                remoteScreenCanvas.msRequestFullscreen();
            }
            isFullScreen = true;
            fullScreenButton.innerHTML = `
                <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 inline-block mr-1" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" />
                </svg>
                Exit Full Screen
            `;
        } else {
            // Exit fullscreen
            if (document.exitFullscreen) {
                document.exitFullscreen();
            } else if (document.webkitExitFullscreen) { /* Safari */
                document.webkitExitFullscreen();
            } else if (document.msExitFullscreen) { /* IE11 */
                document.msExitFullscreen();
            }
            isFullScreen = false;
            fullScreenButton.innerHTML = `
                <svg xmlns="http://www.w3.org/2000/svg" class="h-5 w-5 inline-block mr-1" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 8V4m0 0h4M4 4l5 5m11-1V4m0 0h-4m4 0l-5 5M4 16v4m0 0h4m-4 0l5-5m11 5l-5-5m5 5v-4m0 4h-4" />
                </svg>
                Full Screen
            `;
        }
    } catch (e) {
        console.error('Error toggling fullscreen:', e);
    }
}

/**
 * Close the WebSocket connection and reset UI
 */
function closeConnection() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
    }
    if (controlButtons) {
        controlButtons.classList.add('hidden');
    }
    if (loadingOverlay) {
        loadingOverlay.classList.add('hidden');
    }
    updateStatus('Disconnected', 'info');
    
    // Reset canvas
    if (ctx && remoteScreenCanvas) {
        ctx.clearRect(0, 0, remoteScreenCanvas.width, remoteScreenCanvas.height);
        // Set a default background or message
        ctx.fillStyle = '#000000';
        ctx.fillRect(0, 0, remoteScreenCanvas.width, remoteScreenCanvas.height);
        ctx.fillStyle = '#ffffff';
        ctx.font = '16px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('Disconnected', remoteScreenCanvas.width / 2, remoteScreenCanvas.height / 2);
    }
}

// --- DOMContentLoaded ensures elements are loaded before script tries to access them ---
document.addEventListener('DOMContentLoaded', () => {
    // Initialize DOM Elements references
    connectButton = document.getElementById('connectButton');
    sessionIdInput = document.getElementById('sessionIdInput');
    statusMessage = document.getElementById('statusMessage');
    remoteScreenCanvas = document.getElementById('remoteScreenCanvas');
    loadingOverlay = document.getElementById('loadingOverlay');
    controlButtons = document.getElementById('controlButtons');
    fullScreenButton = document.getElementById('fullScreenButton');
    disconnectButton = document.getElementById('disconnectButton');

    // Set up canvas context
    if (remoteScreenCanvas) {
        ctx = remoteScreenCanvas.getContext('2d');
        // Set initial canvas size
        remoteScreenCanvas.width = 800;
        remoteScreenCanvas.height = 600;
        
        // Draw initial message
        ctx.fillStyle = '#000000';
        ctx.fillRect(0, 0, remoteScreenCanvas.width, remoteScreenCanvas.height);
        ctx.fillStyle = '#ffffff';
        ctx.font = '16px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('Waiting for connection...', remoteScreenCanvas.width / 2, remoteScreenCanvas.height / 2);
    }

    // Add event listeners for control buttons
    if (fullScreenButton) {
        fullScreenButton.addEventListener('click', toggleFullScreen);
    }

    if (disconnectButton) {
        disconnectButton.addEventListener('click', closeConnection);
    }

    // Handle keyboard shortcuts
    document.addEventListener('keydown', (e) => {
        // F11 for fullscreen
        if (e.key === 'F11') {
            e.preventDefault(); // Prevent browser's default F11 behavior
            toggleFullScreen();
        }
        // Escape to exit fullscreen
        if (e.key === 'Escape' && isFullScreen) {
            toggleFullScreen();
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

    // Handle fullscreen change events
    document.addEventListener('fullscreenchange', () => {
        isFullScreen = !!document.fullscreenElement;
    });
    document.addEventListener('webkitfullscreenchange', () => {
        isFullScreen = !!document.webkitFullscreenElement;
    });
    document.addEventListener('msfullscreenchange', () => {
        isFullScreen = !!document.msFullscreenElement;
    });

    // --- Event Listeners for User Interaction ---

    if (connectButton) {
        connectButton.addEventListener('click', () => connectWebSocket('localhost')); // Initial attempt with localhost
    }
    if (sessionIdInput) {
        sessionIdInput.addEventListener('keypress', (event) => {
            if (event.key === 'Enter') {
                connectWebSocket('localhost'); // Initial attempt with localhost
            }
        });
    }

    if (remoteScreenCanvas) {
        remoteScreenCanvas.addEventListener('mousemove', (e) => sendInput('mousemove', { x: e.clientX, y: e.clientY }));
        remoteScreenCanvas.addEventListener('mousedown', (e) => sendInput('mousedown', { x: e.clientX, y: e.clientY, button: e.button }));
        remoteScreenCanvas.addEventListener('mouseup', (e) => sendInput('mouseup', { x: e.clientX, y: e.clientY, button: e.button }));
        remoteScreenCanvas.addEventListener('click', (e) => sendInput('click', { x: e.clientX, y: e.clientY, button: e.button }));
        remoteScreenCanvas.addEventListener('contextmenu', (e) => {
            e.preventDefault();
            sendInput('contextmenu', { x: e.clientX, y: e.clientY, button: e.button });
        });
        remoteScreenCanvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            sendInput('wheel', { deltaY: e.deltaY, x: e.clientX, y: e.clientY });
        });
    }
});
