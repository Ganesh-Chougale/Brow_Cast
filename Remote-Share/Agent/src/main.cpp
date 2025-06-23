#include "WebSocketClient.h"
#include "CaptureManager.h"
#include "ImageProcessor.h"
#include "WindowEnumerator.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <Windows.h>
#include <nlohmann/json.hpp>
int main(int argc, char* argv[]) {
    try {
        ImageProcessor::InitializeCompressor();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error initializing ImageProcessor: " << e.what() << std::endl;
        return 1;
    }
    std::string server_url = "ws://localhost:8080"; // <--- Make sure this is correct
    WebSocketClient ws_client(server_url);
    ws_client.setOnOpenHandler([]() {
        std::cout << "WebSocket connected to server." << std::endl;
    });
    ws_client.setOnCloseHandler([]() {
        std::cerr << "WebSocket disconnected from server. Attempting reconnect (not implemented yet)..." << std::endl;
    });
    ws_client.setOnMessageHandler([](const std::string& message) {
        std::cout << "Received message from server: " << message << std::endl;
    });
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
    CaptureManager captureManager;
    std::vector<WindowInfo> availableWindows = WindowEnumerator::EnumerateVisibleWindows();
    HWND selected_hwnd = WindowEnumerator::SelectWindowFromList(availableWindows);
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
                    frame_msg["image"] = base64_image;
                    frame_msg["width"] = currentWidth;
                    frame_msg["height"] = currentHeight;
                    ws_client.send(frame_msg.dump());
                } else {
                     std::cerr << "Failed to compress image to JPEG." << std::endl;
                }
            } else {
                std::cerr << "Failed to capture pixel data or invalid dimensions (width: " << currentWidth << ", height: " << currentHeight << "). Skipping frame." << std::endl;
            }
        } else {
            std::cerr << "WebSocket not connected. Pausing frame sending." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(66)); 
    }
    ImageProcessor::ShutdownCompressor();
    return 0;
}