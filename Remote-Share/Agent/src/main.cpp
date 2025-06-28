#include "WebSocketClient.h"
#include "CaptureManager.h"
#include "ImageProcessor.h"
#include "WindowEnumerator.h"
#include "InputInjector.h" 
#include <iostream>
#include <string>
#include <limits>
#include <chrono>
#include <thread>
#include <vector>
#include <Windows.h>
#include <nlohmann/json.hpp>

// Define the default session ID. Ensure it's consistent everywhere.
const std::string DEFAULT_SESSION_ID = "def_pas";
const std::string DEFAULT_SERVER_HOST = "localhost"; 
const std::string SERVER_PORT = "8080";

int main(int argc, char* argv[]) {
    std::string server_host; 
    CaptureManager captureManager;
    HWND selected_hwnd = NULL;
    std::vector<WindowInfo> availableWindows;

    // --- Determine Server Host (IP address or hostname) ---
    if (argc > 1) {
        server_host = argv[1]; 
        std::cout << "Using server host from command line: " << server_host << std::endl;
    } else {
        std::string user_input_host;
        std::cout << "\n--- Server Connection Setup ---" << std::endl;
        std::cout << "Enter server IP address: ";
        
        std::cin.clear();
        std::cin.sync();
        std::getline(std::cin, user_input_host);
        
        // Trim whitespace
        user_input_host.erase(0, user_input_host.find_first_not_of(" \t\n\r\f\v"));
        user_input_host.erase(user_input_host.find_last_not_of(" \t\n\r\f\v") + 1);

        if (user_input_host.empty()) {
            server_host = DEFAULT_SERVER_HOST;
            std::cout << "No IP address entered. Connecting to default: " << server_host << std::endl;
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

    // Construct the WebSocket URL
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

    ws_client.setOnMessageHandler([&input_injector, &selected_hwnd](const std::string& message) {
        try {
            auto json_msg = nlohmann::json::parse(message);
            std::string type = json_msg.value("type", "");

            if (type == "input") {
                std::string inputType = json_msg.value("inputType", "");

                if (inputType.rfind("mouse", 0) == 0 || inputType == "click" || 
                    inputType == "contextmenu" || inputType == "wheel") {
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

                    input_injector.InjectKeyboardInput(inputType, key, code, 
                                                     ctrlKey, shiftKey, altKey, metaKey);
                }
            } 
            else if (type == "close_connection") {
                std::cout << "Received close connection command from viewer." << std::endl;
                exit(0);
            }
            else if (type == "toggle_fullscreen") {
                std::cout << "Toggling fullscreen mode." << std::endl;
                selected_hwnd = NULL;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error handling message: " << e.what() << std::endl;
        }
    });

    // --- Connect to WebSocket Server ---
    try {
        ws_client.connect();
    } catch (const std::exception& e) {
        std::cerr << "WebSocket connection failed: " << e.what() << std::endl;
        ImageProcessor::ShutdownCompressor();
        return 1;
    }

    // Wait for connection
    std::cout << "Waiting for WebSocket connection to establish..." << std::endl;
    int connect_timeout_ms = 5000;
    int elapsed_ms = 0;
    while (!ws_client.isConnected() && elapsed_ms < connect_timeout_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        elapsed_ms += 100;
    }

    if (!ws_client.isConnected()) {
        std::cerr << "Failed to connect to WebSocket server." << std::endl;
        ImageProcessor::ShutdownCompressor();
        return 1;
    }

    // --- Window Selection ---
    availableWindows = WindowEnumerator::EnumerateVisibleWindows();
    selected_hwnd = WindowEnumerator::SelectWindowFromList(availableWindows);
    std::cout << "Sharing " << (selected_hwnd == NULL ? "full screen" : "selected window") << std::endl;
    std::cout << "Starting screen sharing (press Ctrl+C to stop)..." << std::endl;

    // --- Main Capture Loop ---
    while (true) { 
        if (ws_client.isConnected()) {
            int currentWidth = 0, currentHeight = 0;
            std::vector<uint8_t> pixel_data = (selected_hwnd == NULL) ?
                captureManager.CaptureFullScreen(currentWidth, currentHeight) :
                captureManager.CaptureWindow(selected_hwnd, currentWidth, currentHeight);

            if (!pixel_data.empty() && currentWidth > 0 && currentHeight > 0) {
                std::vector<uint8_t> jpeg_data = ImageProcessor::CompressToJpeg(
                    pixel_data, currentWidth, currentHeight, 80);
                
                if (!jpeg_data.empty()) {
                    std::string base64_image = ImageProcessor::EncodeToBase64(jpeg_data);
                    nlohmann::json frame_msg = {
                        {"type", "frame"},
                        {"image", "data:image/jpeg;base64," + base64_image},
                        {"width", currentWidth},
                        {"height", currentHeight}
                    };
                    ws_client.send(frame_msg.dump());
                }
            }
        } else {
            std::cerr << "WebSocket disconnected. Attempting to reconnect..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
    }

    ImageProcessor::ShutdownCompressor();
    return 0;
}