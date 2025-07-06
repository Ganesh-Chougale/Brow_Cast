#include "WebSocketClient.hpp"
#include "CaptureManager.hpp"
#include "ImageProcessor.hpp"
#include "WindowEnumerator.hpp"
#include "InputInjector.hpp"

// Windows version definitions are now set in CMakeLists.txt
#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers

#include <iostream>
#include <string>
#include <limits>
#include <chrono>
#include <thread>
#include <vector>
#include <Windows.h>
#include <shellscalingapi.h>  // For DPI-related functions
#include <nlohmann/json.hpp>

// Link against the required Windows libraries
#pragma comment(lib, "shcore.lib")

const std::string DEFAULT_SESSION_ID = "def_pas";
const std::string DEFAULT_SERVER_HOST = "localhost";
const std::string SERVER_PORT = "8080";

int main(int argc, char* argv[]) {
    // Set process DPI awareness for correct scaling behavior
    // Using SetProcessDpiAwareness instead of SetProcessDpiAwarenessContext for MinGW compatibility
    if (HMODULE shcore = LoadLibraryA("shcore.dll")) {
        auto setDpiAwareness = (decltype(&SetProcessDpiAwareness))GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (setDpiAwareness) {
            setDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }
        FreeLibrary(shcore);
    }

    std::string server_host;
    CaptureManager captureManager;
    HWND selected_hwnd = NULL;
    std::vector<WindowInfo> availableWindows;

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

    try {
        ImageProcessor::InitializeCompressor();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error initializing ImageProcessor: " << e.what() << std::endl;
        return 1;
    }

    std::string server_url = "ws://" + server_host + ":" + SERVER_PORT;
    std::cout << "Attempting to connect to WebSocket server at: " << server_url << std::endl;

    WebSocketClient ws_client(server_url);
    InputInjector input_injector;

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

    try {
        ws_client.connect();
    } catch (const std::exception& e) {
        std::cerr << "WebSocket connection failed: " << e.what() << std::endl;
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
        std::cerr << "Failed to connect to WebSocket server." << std::endl;
        ImageProcessor::ShutdownCompressor();
        return 1;
    }

    availableWindows = WindowEnumerator::EnumerateVisibleWindows();
    selected_hwnd = WindowEnumerator::SelectWindowFromList(availableWindows);

    std::cout << "Sharing " << (selected_hwnd == NULL ? "full screen" : "selected window") << std::endl;
    std::cout << "Starting screen sharing (press Ctrl+C to stop)..." << std::endl;

    if (ws_client.isConnected()) {
        // Get actual DPI for the screen or selected window
        UINT currentDpiX = 0, currentDpiY = 0;
        UINT (WINAPI *GetDpiForWindow)(HWND hwnd) = nullptr;
        if (HMODULE user32 = LoadLibraryA("user32.dll")) {
            GetDpiForWindow = (decltype(GetDpiForWindow))GetProcAddress(user32, "GetDpiForWindow");
        }

        if (selected_hwnd == NULL) {
            // For full screen, get DPI of the primary monitor
            HDC screenDC = GetDC(NULL);
            if (screenDC) {
                currentDpiX = GetDeviceCaps(screenDC, LOGPIXELSX);
                currentDpiY = GetDeviceCaps(screenDC, LOGPIXELSY);
                ReleaseDC(NULL, screenDC);
            }
        } else {
            // For a specific window, get its DPI
            if (GetDpiForWindow) {
                currentDpiX = GetDpiForWindow(selected_hwnd);
            } else {
                // Fallback to system DPI if GetDpiForWindow is not available
                HDC hdc = GetDC(NULL);
                currentDpiX = GetDeviceCaps(hdc, LOGPIXELSX);
                ReleaseDC(NULL, hdc);
            }
            currentDpiY = currentDpiX; // Assume square pixels for DPI for simplicity
        }

        int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (screenWidth <= 0 || screenHeight <= 0) {
            screenWidth = GetSystemMetrics(SM_CXSCREEN);
            screenHeight = GetSystemMetrics(SM_CYSCREEN);
        }

        // Calculate scale based on the captured content's DPI
        // Use 96.0f as the standard DPI for 100% scaling
        float scaleX = (currentDpiX > 0) ? (currentDpiX / 96.0f) : 1.0f;
        float scaleY = (currentDpiY > 0) ? (currentDpiY / 96.0f) : 1.0f; // Not strictly needed if X and Y DPI are the same, but good practice

        nlohmann::json screen_info = {
            {"type", "screen_info"},
            {"width", screenWidth},
            {"height", screenHeight},
            {"scaleX", scaleX}, // <--- Send scaleX and scaleY explicitly
            {"scaleY", scaleY}  // <---
        };
        ws_client.send(screen_info.dump());
        std::cout << "Sent screen info to server: " << screenWidth << "x" << screenHeight
                  << " (scaleX: " << scaleX << ", scaleY: " << scaleY << ")" << std::endl;
    }

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
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    ImageProcessor::ShutdownCompressor();
    return 0;
}