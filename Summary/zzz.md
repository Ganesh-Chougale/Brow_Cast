Remote-Share\Agent\CMakeLists.txt:
```text
cmake_minimum_required(VERSION 3.10)
project(RemoteShareAgent CXX)
# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Set Boost debug output
set(Boost_DEBUG ON)
# Find dependencies
find_package(Boost 1.78.0 REQUIRED COMPONENTS thread system)
# Find libjpeg-turbo from system paths
find_path(JPEG_INCLUDE_DIR jpeglib.h PATHS /mingw64/include)
find_library(JPEG_LIBRARY NAMES jpeg libjpeg PATHS /mingw64/lib)
find_library(TURBOJPEG_LIBRARY NAMES turbojpeg libturbojpeg PATHS /mingw64/lib)
if(NOT JPEG_INCLUDE_DIR OR NOT JPEG_LIBRARY OR NOT TURBOJPEG_LIBRARY)
    message(FATAL_ERROR "libjpeg-turbo not found. Please install it with: pacman -S mingw-w64-x86_64-libjpeg-turbo")
else()
    message(STATUS "Found libjpeg include: ${JPEG_INCLUDE_DIR}")
    message(STATUS "Found libjpeg library: ${JPEG_LIBRARY}")
    message(STATUS "Found turbojpeg library: ${TURBOJPEG_LIBRARY}")
    add_library(jpeg STATIC IMPORTED)
    set_target_properties(jpeg PROPERTIES
        IMPORTED_LOCATION ${JPEG_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${JPEG_INCLUDE_DIR}
    )
    add_library(turbojpeg STATIC IMPORTED)
    set_target_properties(turbojpeg PROPERTIES
        IMPORTED_LOCATION ${TURBOJPEG_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${JPEG_INCLUDE_DIR}
    )
endif()
find_package(nlohmann_json 3.11.2 REQUIRED)
# Set source files
set(AGENT_SRCS
    src/main.cpp
    src/CaptureManager.cpp
    src/ImageProcessor.cpp
    src/WebSocketClient.cpp
    src/WindowEnumerator.cpp
)
# Create executable
add_executable(RemoteShareAgent ${AGENT_SRCS})
# Set include directories
target_include_directories(RemoteShareAgent PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libs
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    C:/msys64/mingw64/include
)
# Link libraries
target_link_libraries(RemoteShareAgent PRIVATE
    jpeg
    turbojpeg
    ${Boost_LIBRARIES}
    nlohmann_json::nlohmann_json
    Gdiplus.lib
    ws2_32
    User32.lib
    Gdi32.lib
)
# Add compile definitions if needed
target_compile_definitions(RemoteShareAgent PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)
```

Remote-Share\Agent\src\CaptureManager.cpp:
```cpp
#include "CaptureManager.h" 
#include <iostream>      
#include <stdexcept>     
#include <algorithm>     
#include <limits>        
#include <vector>        
#include <Windows.h>     
CaptureManager::CaptureManager() {
}
CaptureManager::~CaptureManager() {
}
std::vector<uint8_t> CaptureManager::CaptureFullScreen(int& outWidth, int& outHeight) {
    return CapturePixelsInternal(NULL, outWidth, outHeight);
}
std::vector<uint8_t> CaptureManager::CaptureWindow(HWND hwnd, int& outWidth, int& outHeight) {
    if (hwnd == NULL) {
        std::cerr << "CaptureWindow called with NULL HWND. Using CaptureFullScreen instead." << std::endl;
        return CaptureFullScreen(outWidth, outHeight);
    }
    return CapturePixelsInternal(hwnd, outWidth, outHeight);
}
std::vector<uint8_t> CaptureManager::CapturePixelsInternal(HWND hwnd, int& width, int& height) {
    HDC hdcScreen = NULL;
    HDC hdcCompatible = NULL;
    HBITMAP hBitmap = NULL;
    std::vector<uint8_t> pixels;
    int x_src = 0, y_src = 0; 
    if (hwnd == NULL) { 
        hdcScreen = GetDC(NULL); 
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
    } else { 
        RECT client_rect;
        if (!GetClientRect(hwnd, &client_rect)) {
            std::cerr << "GetClientRect failed for HWND: " << hwnd << ". Error: " << GetLastError() << std::endl;
            return pixels; 
        }
        POINT pt[2];
        pt[0].x = client_rect.left;
        pt[0].y = client_rect.top;
        pt[1].x = client_rect.right;
        pt[1].y = client_rect.bottom;
        if (!MapWindowPoints(hwnd, NULL, pt, 2)) {
             std::cerr << "MapWindowPoints failed for HWND: " << hwnd << ". Error: " << GetLastError() << std::endl;
             return pixels;
        }
        width = pt[1].x - pt[0].x;
        height = pt[1].y - pt[0].y;
        x_src = pt[0].x; 
        y_src = pt[0].y; 
        if (width <= 0 || height <= 0) {
            std::cerr << "Invalid window dimensions: " << width << "x" << height << " for HWND: " << hwnd << std::endl;
            return pixels;
        }
        hdcScreen = GetDC(NULL); 
        if (!hdcScreen) {
            std::cerr << "GetDC(NULL) failed for window capture. Error: " << GetLastError() << std::endl;
            return pixels;
        }
    }
    hdcCompatible = CreateCompatibleDC(hdcScreen);
    if (!hdcCompatible) {
        std::cerr << "CreateCompatibleDC failed! Error: " << GetLastError() << std::endl;
        if (hdcScreen) ReleaseDC(NULL, hdcScreen); 
        return pixels;
    }
    hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    if (!hBitmap) {
        std::cerr << "CreateCompatibleBitmap failed! Error: " << GetLastError() << std::endl;
        DeleteDC(hdcCompatible);
        if (hdcScreen) ReleaseDC(NULL, hdcScreen);
        return pixels;
    }
    SelectObject(hdcCompatible, hBitmap);
    if (hwnd == NULL) { 
        BitBlt(hdcCompatible, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    } else { 
        BOOL success = PrintWindow(hwnd, hdcCompatible, PW_CLIENTONLY);
        if (!success) {
            std::cerr << "PrintWindow failed for HWND: " << hwnd << ". Falling back to BitBlt. Error: " << GetLastError() << std::endl;
            BitBlt(hdcCompatible, 0, 0, width, height, hdcScreen, x_src, y_src, SRCCOPY);
        }
    }
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; 
    bi.biPlanes = 1;
    bi.biBitCount = 24; 
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0; 
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;
    int bytesPerPixel = 3;
    pixels.resize(static_cast<size_t>(width) * height * bytesPerPixel); 
    if (!GetDIBits(hdcCompatible, hBitmap, 0, height, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
        std::cerr << "GetDIBits failed! Error: " << GetLastError() << std::endl;
        pixels.clear(); 
    }
    DeleteObject(hBitmap);
    DeleteDC(hdcCompatible);
    if (hdcScreen) ReleaseDC(NULL, hdcScreen); 
    return pixels;
}
```

Remote-Share\Agent\src\CaptureManager.h:
```cpp
#pragma once 
#include <vector>
#include <cstdint> 
#include <Windows.h> 
class CaptureManager {
public:
    CaptureManager();
    ~CaptureManager();
    std::vector<uint8_t> CaptureFullScreen(int& outWidth, int& outHeight);
    std::vector<uint8_t> CaptureWindow(HWND hwnd, int& outWidth, int& outHeight);
private:
    std::vector<uint8_t> CapturePixelsInternal(HWND hwnd, int& width, int& height);
};
```

Remote-Share\Agent\src\ImageProcessor.cpp:
```cpp
#include "ImageProcessor.h"
#include <iostream>
#include <stdexcept>
#include <vector>
tjhandle ImageProcessor::s_jpegCompressor = nullptr;
static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";
std::string ImageProcessor::base64_encode_impl(const std::vector<uint8_t>& in) {
    std::string out;
    int val = 0, valb = -6;
    for (uint8_t c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        out.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (out.size() % 4) {
        out.push_back('=');
    }
    return out;
}
ImageProcessor::ImageProcessor() {
}
ImageProcessor::~ImageProcessor() {
}
void ImageProcessor::InitializeCompressor() {
    if (!s_jpegCompressor) {
        s_jpegCompressor = tjInitCompress();
        if (!s_jpegCompressor) {
            const char* error_str = tjGetErrorStr();
            std::cerr << "Failed to initialize libjpeg-turbo compressor: " << (error_str ? error_str : "Unknown error") << std::endl;
            throw std::runtime_error("Failed to initialize libjpeg-turbo compressor.");
        }
    }
}
void ImageProcessor::ShutdownCompressor() {
    if (s_jpegCompressor) {
        tjDestroy(s_jpegCompressor);
        s_jpegCompressor = nullptr;
    }
}
std::vector<uint8_t> ImageProcessor::CompressToJpeg(const std::vector<uint8_t>& pixelData, int width, int height, int quality) {
    std::vector<uint8_t> jpegData;
    if (!s_jpegCompressor) {
        std::cerr << "libjpeg-turbo compressor not initialized." << std::endl;
        return jpegData;
    }
    if (pixelData.empty() || width <= 0 || height <= 0) {
        std::cerr << "Invalid pixel data or dimensions for JPEG compression." << std::endl;
        return jpegData;
    }
    unsigned char* jpegBuf = NULL; 
    unsigned long jpegSize = 0;    
    int pixelFormat = TJPF_BGR; 
    int result = tjCompress2(s_jpegCompressor,
                             pixelData.data(),      
                             width,                 
                             width * 3,             
                             height,                
                             pixelFormat,           
                             &jpegBuf,              
                             &jpegSize,             
                             TJSAMP_420,            
                             quality,               
                             TJFLAG_FASTDCT         
                            );
    if (result != 0) {
        const char* error_str = tjGetErrorStr();
        std::cerr << "Failed to compress image with libjpeg-turbo: " << (error_str ? error_str : "Unknown error") << std::endl;
        if (jpegBuf) { 
            tjFree(jpegBuf);
        }
        return jpegData;
    }
    jpegData.assign(jpegBuf, jpegBuf + jpegSize);
    tjFree(jpegBuf);
    return jpegData;
}
std::string ImageProcessor::EncodeToBase64(const std::vector<uint8_t>& binaryData) {
    return base64_encode_impl(binaryData);
}
```

Remote-Share\Agent\src\ImageProcessor.h:
```cpp
#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H
#include <vector>
#include <string>
#include <cstdint>
#include <turbojpeg.h> 
class ImageProcessor {
public:
    ImageProcessor();
    ~ImageProcessor();
    static void InitializeCompressor();
    static void ShutdownCompressor();
    static std::vector<uint8_t> CompressToJpeg(const std::vector<uint8_t>& pixelData, int width, int height, int quality = 80);
    static std::string EncodeToBase64(const std::vector<uint8_t>& binaryData);
private:
    static tjhandle s_jpegCompressor;
    static std::string base64_encode_impl(const std::vector<uint8_t>& in);
};
#endif
```

Remote-Share\Agent\src\main.cpp:
```cpp
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
    std::string server_url = "ws:
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
```

Remote-Share\Agent\src\WebSocketClient.cpp:
```cpp
#include "WebSocketClient.h"
#include <iostream>
WebSocketClient::WebSocketClient(const std::string& uri)
    : m_uri(uri), m_connected(false) {
    m_client.init_asio();
    m_client.set_access_channels(websocketpp::log::alevel::none);
    m_client.set_error_channels(websocketpp::log::elevel::all);
    m_client.set_open_handler(websocketpp::lib::bind(
        &WebSocketClient::onOpen,
        this,
        websocketpp::lib::placeholders::_1
    ));
    m_client.set_close_handler(websocketpp::lib::bind(
        &WebSocketClient::onClose,
        this,
        websocketpp::lib::placeholders::_1
    ));
    m_client.set_message_handler(websocketpp::lib::bind(
        &WebSocketClient::onMessage,
        this,
        websocketpp::lib::placeholders::_1,
        websocketpp::lib::placeholders::_2
    ));
    m_client.set_fail_handler(websocketpp::lib::bind(
        &WebSocketClient::onFail,
        this,
        websocketpp::lib::placeholders::_1
    ));
}
WebSocketClient::~WebSocketClient() {
    if (m_connected.load()) {
        websocketpp::lib::error_code ec;
        if (m_hdl.expired()) {
            std::cerr << "WebSocketClient destructor: Connection handle expired, skipping close." << std::endl;
        } else {
            m_client.close(m_hdl, websocketpp::close::status::going_away, "Client shutting down", ec);
            if (ec) {
                std::cerr << "Error closing WebSocket: " << ec.message() << std::endl;
            }
        }
    }
    m_client.stop();
    if (m_thread.joinable()) {
        m_thread.join();
    }
}
void WebSocketClient::connect() {
    websocketpp::lib::error_code ec;
    client::connection_ptr con = m_client.get_connection(m_uri, ec);
    if (ec) {
        std::cerr << "Could not create connection because: " << ec.message() << std::endl;
        throw websocketpp::exception(ec.message()); 
    }
    m_client.connect(con);
    if (!m_thread.joinable()) {
        m_thread = websocketpp::lib::thread([&]() {
            try {
                m_client.run();
            } catch (const websocketpp::exception& e) {
                std::cerr << "WebSocket client run exception: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "General exception in WebSocket client run thread: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in WebSocket client run thread." << std::endl;
            }
        });
    }
}
void WebSocketClient::send(const std::string& message_payload) {
    if (m_connected.load() && !m_hdl.expired()) {
        websocketpp::lib::error_code ec;
        m_client.send(m_hdl, message_payload, websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cerr << "Error sending message: " << ec.message() << std::endl;
        }
    } else {
    }
}
bool WebSocketClient::isConnected() const {
    return m_connected.load() && !m_hdl.expired();
}
void WebSocketClient::setOnOpenHandler(std::function<void()> handler) {
    m_onOpenHandler = handler;
}
void WebSocketClient::setOnCloseHandler(std::function<void()> handler) {
    m_onCloseHandler = handler;
}
void WebSocketClient::setOnMessageHandler(std::function<void(const std::string&)> handler) {
    m_onMessageHandler = handler;
}
void WebSocketClient::onOpen(websocketpp::connection_hdl hdl) {
    m_hdl = hdl; 
    m_connected.store(true); 
    if (m_onOpenHandler) {
        m_onOpenHandler(); 
    }
}
void WebSocketClient::onClose(websocketpp::connection_hdl hdl) {
    m_connected.store(false); 
    if (m_onCloseHandler) {
        m_onCloseHandler(); 
    }
}
void WebSocketClient::onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg) {
    if (m_onMessageHandler) {
        m_onMessageHandler(msg->get_payload()); 
    }
}
void WebSocketClient::onFail(websocketpp::connection_hdl hdl) {
    client::connection_ptr con = m_client.get_con_from_hdl(hdl);
    std::cerr << "WebSocket connection failed: " << con->get_ec().message() << std::endl;
    m_connected.store(false); 
    if (m_onCloseHandler) {
        m_onCloseHandler();
    }
}
```

Remote-Share\Agent\src\WebSocketClient.h:
```cpp
#pragma once
#include <string>
#include <vector>
#include <functional> 
#include <thread>     
#include <atomic>     
#define ASIO_STANDALONE 
#include <asio.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp> 
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp> 
#include <websocketpp/common/memory.hpp> 
#include <nlohmann/json.hpp>
typedef websocketpp::client<websocketpp::config::asio_client> client;
class WebSocketClient {
public:
    WebSocketClient(const std::string& uri);
    ~WebSocketClient();
    void connect();
    void send(const std::string& message_payload);
    bool isConnected() const;
    void setOnOpenHandler(std::function<void()> handler);
    void setOnCloseHandler(std::function<void()> handler);
    void setOnMessageHandler(std::function<void(const std::string&)> handler);
private:
    client m_client; 
    websocketpp::connection_hdl m_hdl; 
    std::string m_uri; 
    std::atomic<bool> m_connected; 
    websocketpp::lib::thread m_thread; 
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, client::message_ptr msg);
    void onFail(websocketpp::connection_hdl hdl);
    std::function<void()> m_onOpenHandler;
    std::function<void()> m_onCloseHandler;
    std::function<void(const std::string&)> m_onMessageHandler;
};
```

Remote-Share\Agent\src\WindowEnumerator.cpp:
```cpp
#include "WindowEnumerator.h" 
#include <algorithm> 
#include <iostream>  
#include <limits>    
std::vector<WindowInfo>* WindowEnumerator::s_currentWindowsList = nullptr;
BOOL CALLBACK WindowEnumerator::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (s_currentWindowsList == nullptr) {
        return FALSE; 
    }
    if (IsWindowVisible(hwnd) && GetWindowTextLength(hwnd) != 0) {
        char buffer[512]; 
        GetWindowTextA(hwnd, buffer, sizeof(buffer)); 
        std::string title(buffer); 
        if (title.empty() ||
            title == "Program Manager" ||         
            title == "Default IME" ||             
            title == "MSCTFIME UI" ||             
            title.find("NVIDIA GeForce Overlay") != std::string::npos || 
            title.find("GDI+ Window") != std::string::npos ||         
            title.find("Qt Message Window") != std::string::npos ||   
            title.find("ApplicationManagerWindow") != std::string::npos || 
            title.find("Cortana") != std::string::npos ||            
            title.find("Windows Input Experience") != std::string::npos || 
            title.length() < 3 || 
            (GetConsoleWindow() == hwnd && !IsWindowVisible(hwnd)) 
           )
        {
            return TRUE; 
        }
        s_currentWindowsList->push_back({hwnd, title});
    }
    return TRUE; 
}
std::vector<WindowInfo> WindowEnumerator::EnumerateVisibleWindows() {
    std::vector<WindowInfo> windows;
    s_currentWindowsList = &windows; 
    EnumWindows(EnumWindowsProc, NULL); 
    s_currentWindowsList = nullptr; 
    std::sort(windows.begin(), windows.end(), [](const WindowInfo& a, const WindowInfo& b) {
        return a.title < b.title;
    });
    return windows;
}
HWND WindowEnumerator::SelectWindowFromList(const std::vector<WindowInfo>& windows) {
    std::cout << "\n--- Select a Window to Share ---" << std::endl;
    std::cout << "0. Full Screen" << std::endl; 
    for (size_t i = 0; i < windows.size(); ++i) {
        std::cout << i + 1 << ". " << windows[i].title << std::endl;
    }
    std::cout << "Enter selection (0-" << windows.size() << "): ";
    int choice;
    while (!(std::cin >> choice) || choice < 0 || choice > static_cast<int>(windows.size())) {
        std::cin.clear(); 
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 
        std::cerr << "Invalid input. Please enter a number between 0 and " << windows.size() << ": ";
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); 
    if (choice == 0) {
        std::cout << "Sharing full screen." << std::endl;
        return NULL; 
    } else {
        HWND selectedHwnd = windows[choice - 1].hwnd; 
        std::cout << "Sharing: " << windows[choice - 1].title << std::endl;
        return selectedHwnd;
    }
}
```

Remote-Share\Agent\src\WindowEnumerator.h:
```cpp
#pragma once 
#include <Windows.h> 
#include <string>    
#include <vector>    
struct WindowInfo {
    HWND hwnd;
    std::string title;
};
class WindowEnumerator {
public:
    static std::vector<WindowInfo> EnumerateVisibleWindows();
    static HWND SelectWindowFromList(const std::vector<WindowInfo>& windows);
private:
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
    static std::vector<WindowInfo>* s_currentWindowsList;
};
```

Remote-Share\Server\index.js:
```js
const express = require('express');
const WebSocket = require('ws');
const http = require('http');
const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });
const PORT = process.env.PORT || 8080;
app.use(express.static('../Web')); 
const webClients = new Set();
let agentWs = null; 
wss.on('connection', ws => {
    if (!agentWs) {
        agentWs = ws;
        console.log('Client connected (Agent)');
        ws.on('message', message => {
            for (const client of webClients) {
                if (client.readyState === WebSocket.OPEN) {
                    client.send(message); 
                }
            }
        });
        ws.on('close', () => {
            console.log('Client (Agent) disconnected');
            agentWs = null; 
        });
        ws.on('error', error => {
            console.error('WebSocket error with Agent:', error);
            agentWs = null;
        });
    } else {
        webClients.add(ws);
        console.log('Web client connected');
        ws.on('close', () => {
            console.log('Web client disconnected');
            webClients.delete(ws); 
        });
        ws.on('error', error => {
            console.error('WebSocket error with Web Client:', error);
            webClients.delete(ws);
        });
        ws.on('message', message => {
            console.log('Received message from web client:', message.toString());
        });
    }
});
server.listen(PORT, () => {
    console.log(`HTTP and WebSocket server listening on port ${PORT}`);
    console.log(`Access web client at http:
});
```

Remote-Share\Web\index.html:
```html
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Remote Share Viewer</title>
    <link rel="stylesheet" href="./style.css">
</head>
<body>
    <canvas id="remoteCanvas"></canvas>
    <script src="./script.js"></script>
</body>
</html>
```

Remote-Share\Web\script.js:
```js
document.addEventListener('DOMContentLoaded', () => {
    const canvas = document.getElementById('remoteCanvas');
    const ctx = canvas.getContext('2d');
    const wsUrl = `ws:
    const ws = new WebSocket(wsUrl);
    ws.onopen = () => {
        console.log('WebSocket connection established with server.');
    };
    ws.onmessage = event => {
        try {
            const message = JSON.parse(event.data);
            if (message.type === 'frame' && message.image) {
                const img = new Image();
                img.onload = () => {
                    canvas.width = message.width;
                    canvas.height = message.height;
                    ctx.drawImage(img, 0, 0, message.width, message.height);
                };
                img.src = `data:image/jpeg;base64,${message.image}`;
            }
        } catch (error) {
            console.error('Error parsing or processing WebSocket message:', error);
        }
    };
    ws.onclose = () => {
        console.log('WebSocket connection closed.');
    };
    ws.onerror = error => {
        console.error('WebSocket error:', error);
    };
    function resizeCanvas() {
    }
    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);
});
```

Remote-Share\Web\style.css:
```css

```

