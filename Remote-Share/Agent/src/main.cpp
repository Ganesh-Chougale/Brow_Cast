#include "WebSocketClient.h"
#include "CaptureManager.h"
#include "ImageProcessor.h"
#include "WindowEnumerator.h"
#include "InputInjector.h" 
#include <iostream>     // For std::cout, std::cerr, std::cin
#include <string>       // For std::string
#include <limits>       // For std::numeric_limits
#include <chrono>
#include <thread>
#include <vector>
#include <Windows.h>
#include <nlohmann/json.hpp>

// Define the default session ID. Ensure it's consistent everywhere.
const std::string DEFAULT_SESSION_ID = "def_pas"; // Changed to "def_pas"
const std::string DEFAULT_SERVER_HOST = "localhost"; 
const std::string SERVER_PORT = "8080";

int main(int argc, char* argv[]) {
    std::string server_host; 

    // --- Determine Server Host (IP address or hostname) ---
    if (argc > 1) {
        // Option 1: Server host provided as a command-line argument
        server_host = argv[1]; 
        std::cout << "Using server host from command line: " << server_host << std::endl;
    } else {
        // Option 2: No command-line argument, prompt user for IP
        std::string user_input_host;
        std::cout << "\n--- Server Connection Setup ---" << std::endl;
        std::cout << "Enter server IP address: ";
        
        // Clear any previous errors and flush the input buffer
        std::cin.clear();
        std::cin.sync();
        
        // Read the input line
        std::getline(std::cin, user_input_host);
        
        // Trim whitespace from the input
        user_input_host.erase(0, user_input_host.find_first_not_of(" \t\n\r\f\v"));
        user_input_host.erase(user_input_host.find_last_not_of(" \t\n\r\f\v") + 1);

        if (user_input_host.empty()) {
            server_host = DEFAULT_SERVER_HOST;
            std::cout << "No IP address entered. Connecting to default: " << server_host << std::endl;
            std::cout << "Note: If Node.js server is on another device, use its actual IP address." << std::endl;
        } else {
            server_host = user_input_host;
            std::cout << "Connecting to: " << server_host << std::endl;
        }
    }

    // --- Initialize Image Processor ---
    try {
        ImageProcessor::InitializeCompressor();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error initializing ImageProcessor: " << e.what() << std::endl;
        return 1;
    }

    // Construct the full server URL for WebSocket connection
    // Ensure the sessionId matches what the server is expecting from the client (no #).
    std::string server_url = "ws://" + server_host + ":" + SERVER_PORT + "/agent?sessionId=" + DEFAULT_SESSION_ID;
    std::cout << "Attempting to connect to WebSocket server at: " << server_url << std::endl;

    // Create WebSocket client and InputInjector instances
    WebSocketClient ws_client(server_url);
    InputInjector input_injector;

    // --- Set WebSocket Event Handlers ---
    ws_client.setOnOpenHandler([]() {
        std::cout << "WebSocket connected to server." << std::endl;
    });

    ws_client.setOnCloseHandler([]() {
        std::cerr << "WebSocket disconnected from server. Attempting reconnect..." << std::endl; 
    });

    ws_client.setOnMessageHandler([&input_injector](const std::string& message) {
        try {
            auto json_msg = nlohmann::json::parse(message);
            std::string type = json_msg.value("type", "");

            if (type == "input") {
                std::string inputType = json_msg.value("inputType", "");

                if (inputType.rfind("mouse", 0) == 0 || inputType == "click" || inputType == "contextmenu" || inputType == "wheel") {
                    int x = json_msg.value("x", 0);
                    int y = json_msg.value("y", 0);
                    int button = json_msg.value("button", -1);
                    int deltaY = json_msg.value("deltaY", 0);

                    input_injector.InjectMouseInput(inputType, x, y, button, deltaY);
                } 
                else if (inputType.rfind("key", 0) == 0) {
                    std::string key = json_msg.value("key", "");
                    std::string code = json_msg.value("code", "");
                    bool ctrlKey = json_msg.value("ctrlKey", false);
                    bool shiftKey = json_msg.value("shiftKey", false);
                    bool altKey = json_msg.value("altKey", false);
                    bool metaKey = json_msg.value("metaKey", false);

                    input_injector.InjectKeyboardInput(inputType, key, code, ctrlKey, shiftKey, altKey, metaKey);
                } else {
                    std::cerr << "Unknown inputType received: " << inputType << std::endl;
                }
            } else {
                std::cout << "Received non-input message from server: " << message << std::endl;
            }
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "JSON parsing error for incoming message: " << e.what() << " - Message (truncated): " << message.substr(0, 100) << "..." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error handling incoming message: " << e.what() << " - Message (truncated): " << message.substr(0, 100) << "..." << std::endl;
        }
    });

    // --- Connect to WebSocket Server ---
    try {
        ws_client.connect();
    } catch (const std::exception& e) {
        std::cerr << "Initial WebSocket connection setup failed: " << e.what() << std::endl;
        ImageProcessor::ShutdownCompressor();
        return 1;
    }

    std::cout << "Waiting for WebSocket connection to establish..." << std::endl;
    int connect_timeout_ms = 5000;
    int elapsed_ms = 0;
    while (!ws_client.isConnected() && elapsed_ms < connect_timeout_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        elapsed_ms += 100;
    }

    if (!ws_client.isConnected()) {
        std::cerr << "Failed to connect to WebSocket server within timeout. Ensure Node.js server is running on " << server_url << std::endl;
        ImageProcessor::ShutdownCompressor();
        return 1;
    }

    // --- Screen Capture and Streaming Loop ---
    CaptureManager captureManager;
    
    // Enumerate visible windows and allow user to select one for sharing
    std::vector<WindowInfo> availableWindows = WindowEnumerator::EnumerateVisibleWindows();
    
    // After selecting window, WindowEnumerator::SelectWindowFromList handles clearing cin buffer.
    // So, we don't need another cin.ignore here.
    HWND selected_hwnd = WindowEnumerator::SelectWindowFromList(availableWindows);

    std::cout << "Sharing " << (selected_hwnd == NULL ? "full screen" : "selected window") << "." << std::endl;
    std::cout << "Starting screen sharing loop (press Ctrl+C to stop)..." << std::endl;

    while (true) { 
        if (ws_client.isConnected()) {
            int currentWidth = 0, currentHeight = 0;
            std::vector<uint8_t> pixel_data;

            if (selected_hwnd == NULL) {
                pixel_data = captureManager.CaptureFullScreen(currentWidth, currentHeight);
            } else {
                pixel_data = captureManager.CaptureWindow(selected_hwnd, currentWidth, currentHeight);
            }

            if (!pixel_data.empty() && currentWidth > 0 && currentHeight > 0) {
                std::vector<uint8_t> jpeg_data = ImageProcessor::CompressToJpeg(pixel_data, currentWidth, currentHeight, 80);
                
                if (!jpeg_data.empty()) {
                    std::string base64_image = ImageProcessor::EncodeToBase64(jpeg_data);

                    nlohmann::json frame_msg;
                    frame_msg["type"] = "frame";
                    frame_msg["image"] = "data:image/jpeg;base64," + base64_image;
                    frame_msg["width"] = currentWidth;
                    frame_msg["height"] = currentHeight;

                    ws_client.send(frame_msg.dump());
                } else {
                    std::cerr << "Failed to compress image to JPEG. Skipping frame." << std::endl;
                }
            } else {
                // Keep this commented to avoid excessive console spam for transient issues.
                // std::cerr << "Failed to capture pixel data or invalid dimensions (width: " << currentWidth << ", height: " << currentHeight << "). Skipping frame." << std::endl;
            }
        } else {
            std::cerr << "WebSocket not connected. Pausing frame sending and attempting reconnect." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(66)); 
    }

    ImageProcessor::ShutdownCompressor();
    return 0;
}
