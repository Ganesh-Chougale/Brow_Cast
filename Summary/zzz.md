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
    src/InputInjector.cpp
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

Remote-Share\Agent\src\InputInjector.cpp:
```cpp
#include "InputInjector.h"
#include <iostream>
#pragma comment(lib, "user32.lib") 
InputInjector::InputInjector() {
    InitializeKeyMap();
}
void InputInjector::SendInputEvents(const std::vector<INPUT>& inputs) {
    if (inputs.empty()) return;
    UINT successfulInputs = SendInput(inputs.size(), (LPINPUT)inputs.data(), sizeof(INPUT));
    if (successfulInputs != inputs.size()) {
        std::cerr << "Warning: SendInput failed to inject all events. Only " << successfulInputs 
                  << " out of " << inputs.size() << " were injected. Error: " << GetLastError() << std::endl;
    }
}
int InputInjector::GetScreenWidth() {
    return GetSystemMetrics(SM_CXSCREEN);
}
int InputInjector::GetScreenHeight() {
    return GetSystemMetrics(SM_CYSCREEN);
}
void InputInjector::InjectMouseInput(const std::string& inputType, int x, int y, int button, int deltaY) {
    INPUT input = {0};
    input.type = INPUT_MOUSE;
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    if (screenWidth == 0 || screenHeight == 0) {
        std::cerr << "Error: Screen dimensions are zero. Cannot inject mouse input." << std::endl;
        return;
    }
    input.mi.dx = (x * 65535) / screenWidth;
    input.mi.dy = (y * 65535) / screenHeight;
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    if (inputType == "mousemove") {
        input.mi.dwFlags |= MOUSEEVENTF_MOVE;
    } else if (inputType == "mousedown") {
        if (button == 0) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;   
        else if (button == 1) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN; 
        else if (button == 2) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;  
    } else if (inputType == "mouseup") {
        if (button == 0) input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;     
        else if (button == 1) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;   
        else if (button == 2) input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;    
    } else if (inputType == "click") {
        INPUT down_input = input;
        if (button == 0) down_input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        else if (button == 1) down_input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
        else if (button == 2) down_input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        INPUT up_input = input;
        if (button == 0) up_input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
        else if (button == 1) up_input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
        else if (button == 2) up_input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
        SendInputEvents({down_input, up_input});
        return; 
    } else if (inputType == "contextmenu") { 
        input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        INPUT up_input = input;
        up_input.mi.dwFlags = MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        SendInputEvents({input, up_input});
        return;
    } else if (inputType == "wheel") {
        input.mi.dwFlags |= MOUSEEVENTF_WHEEL;
        input.mi.mouseData = (DWORD)(deltaY > 0 ? WHEEL_DELTA : -WHEEL_DELTA);
        if (std::abs(deltaY) > 0) { 
             input.mi.mouseData = (DWORD)((deltaY / 100.0) * WHEEL_DELTA); 
        } else {
             input.mi.mouseData = (DWORD)deltaY; 
        }
    } else {
        std::cerr << "Unknown mouse input type: " << inputType << std::endl;
        return;
    }
    SendInputEvents({input}); 
}
void InputInjector::InjectKeyboardInput(const std::string& inputType, const std::string& key, const std::string& code,
                                         bool ctrlKey, bool shiftKey, bool altKey, bool metaKey) {
    std::vector<INPUT> inputs;
    INPUT keyboardInput = {0};
    keyboardInput.type = INPUT_KEYBOARD;
    WORD vkCode = 0;
    auto it = browserCodeToVkMap.find(code);
    if (it != browserCodeToVkMap.end()) {
        vkCode = it->second;
        keyboardInput.ki.wScan = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
        keyboardInput.ki.dwFlags = KEYEVENTF_SCANCODE;
        if (code == "NumpadEnter" || code.find("Right") != std::string::npos || 
            code == "ControlRight" || code == "AltRight" || code == "Insert" || 
            code == "Delete" || code == "Home" || code == "End" || code == "PageUp" || 
            code == "PageDown" || code == "ArrowUp" || code == "ArrowDown" || 
            code == "ArrowLeft" || code == "ArrowRight" || code == "NumpadDivide" || code == "PrintScreen") {
            keyboardInput.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
    } else {
        if (key.length() == 1 && std::isalpha(key[0])) { 
             vkCode = toupper(key[0]);
        } else if (key.length() == 1 && std::isdigit(key[0])) { 
             vkCode = key[0];
        } else if (key == "Backspace") vkCode = VK_BACK;
        else if (key == "Tab") vkCode = VK_TAB;
        else if (key == "Enter") vkCode = VK_RETURN;
        else if (key == "Shift") vkCode = VK_SHIFT; 
        else if (key == "Control") vkCode = VK_CONTROL; 
        else if (key == "Alt") vkCode = VK_MENU; 
        else if (key == "Pause") vkCode = VK_PAUSE;
        else if (key == "CapsLock") vkCode = VK_CAPITAL;
        else if (key == "Escape") vkCode = VK_ESCAPE;
        else if (key == "Space") vkCode = VK_SPACE;
        else if (key == "PageUp") vkCode = VK_PRIOR;
        else if (key == "PageDown") vkCode = VK_NEXT;
        else if (key == "End") vkCode = VK_END;
        else if (key == "Home") vkCode = VK_HOME;
        else if (key == "ArrowLeft") vkCode = VK_LEFT;
        else if (key == "ArrowUp") vkCode = VK_UP;
        else if (key == "ArrowRight") vkCode = VK_RIGHT;
        else if (key == "ArrowDown") vkCode = VK_DOWN;
        else if (key == "PrintScreen") vkCode = VK_SNAPSHOT;
        else if (key == "Insert") vkCode = VK_INSERT;
        else if (key == "Delete") vkCode = VK_DELETE;
        else if (key == "Meta") vkCode = VK_LWIN; 
        else if (key.find("F") == 0 && key.length() > 1 && std::isdigit(key[1])) { 
            int fNum = std::stoi(key.substr(1));
            if (fNum >= 1 && fNum <= 24) vkCode = VK_F1 + (fNum - 1);
        } else if (key == "NumLock") vkCode = VK_NUMLOCK;
        else if (key == "ScrollLock") vkCode = VK_SCROLL;
        keyboardInput.ki.wVk = vkCode;
        keyboardInput.ki.dwFlags = 0; 
        if (vkCode == VK_INSERT || vkCode == VK_DELETE || vkCode == VK_HOME || vkCode == VK_END ||
            vkCode == VK_PRIOR || vkCode == VK_NEXT || vkCode == VK_UP || vkCode == VK_DOWN ||
            vkCode == VK_LEFT || vkCode == VK_RIGHT || vkCode == VK_NUMLOCK || vkCode == VK_DIVIDE ||
            vkCode == VK_RETURN && keyboardInput.ki.wScan == MapVirtualKey(VK_RETURN, MAPVK_VK_TO_VSC)) { 
            keyboardInput.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
    }
    if (vkCode == 0) {
        std::cerr << "Warning: Could not map browser key '" << key << "' (code: '" << code << "') to a Windows virtual key code. Skipping injection." << std::endl;
        return;
    }
    if (inputType == "keyup") {
        keyboardInput.ki.dwFlags |= KEYEVENTF_KEYUP;
    }
    inputs.push_back(keyboardInput);
    SendInputEvents(inputs);
}
void InputInjector::InitializeKeyMap() {
    for (char c = 'A'; c <= 'Z'; ++c) {
        browserCodeToVkMap["Key" + std::string(1, c)] = c;
    }
    for (char c = '0'; c <= '9'; ++c) {
        browserCodeToVkMap["Digit" + std::string(1, c)] = c;
    }
    for (int i = 1; i <= 24; ++i) {
        browserCodeToVkMap["F" + std::to_string(i)] = VK_F1 + (i - 1);
    }
    browserCodeToVkMap["Numpad0"] = VK_NUMPAD0;
    browserCodeToVkMap["Numpad1"] = VK_NUMPAD1;
    browserCodeToVkMap["Numpad2"] = VK_NUMPAD2;
    browserCodeToVkMap["Numpad3"] = VK_NUMPAD3;
    browserCodeToVkMap["Numpad4"] = VK_NUMPAD4;
    browserCodeToVkMap["Numpad5"] = VK_NUMPAD5;
    browserCodeToVkMap["Numpad6"] = VK_NUMPAD6;
    browserCodeToVkMap["Numpad7"] = VK_NUMPAD7;
    browserCodeToVkMap["Numpad8"] = VK_NUMPAD8;
    browserCodeToVkMap["Numpad9"] = VK_NUMPAD9;
    browserCodeToVkMap["NumpadMultiply"] = VK_MULTIPLY;
    browserCodeToVkMap["NumpadAdd"] = VK_ADD;
    browserCodeToVkMap["NumpadSubtract"] = VK_SUBTRACT;
    browserCodeToVkMap["NumpadDecimal"] = VK_DECIMAL;
    browserCodeToVkMap["NumpadDivide"] = VK_DIVIDE;
    browserCodeToVkMap["NumpadEnter"] = VK_RETURN; 
    browserCodeToVkMap["Backspace"] = VK_BACK;
    browserCodeToVkMap["Tab"] = VK_TAB;
    browserCodeToVkMap["Enter"] = VK_RETURN;
    browserCodeToVkMap["ShiftLeft"] = VK_LSHIFT;
    browserCodeToVkMap["ShiftRight"] = VK_RSHIFT;
    browserCodeToVkMap["ControlLeft"] = VK_LCONTROL;
    browserCodeToVkMap["ControlRight"] = VK_RCONTROL;
    browserCodeToVkMap["AltLeft"] = VK_LMENU; 
    browserCodeToVkMap["AltRight"] = VK_RMENU; 
    browserCodeToVkMap["MetaLeft"] = VK_LWIN; 
    browserCodeToVkMap["MetaRight"] = VK_RWIN; 
    browserCodeToVkMap["Escape"] = VK_ESCAPE;
    browserCodeToVkMap["Space"] = VK_SPACE;
    browserCodeToVkMap["PageUp"] = VK_PRIOR;
    browserCodeToVkMap["PageDown"] = VK_NEXT;
    browserCodeToVkMap["End"] = VK_END;
    browserCodeToVkMap["Home"] = VK_HOME;
    browserCodeToVkMap["ArrowLeft"] = VK_LEFT;
    browserCodeToVkMap["ArrowUp"] = VK_UP;
    browserCodeToVkMap["ArrowRight"] = VK_RIGHT;
    browserCodeToVkMap["ArrowDown"] = VK_DOWN;
    browserCodeToVkMap["Insert"] = VK_INSERT;
    browserCodeToVkMap["Delete"] = VK_DELETE;
    browserCodeToVkMap["CapsLock"] = VK_CAPITAL;
    browserCodeToVkMap["NumLock"] = VK_NUMLOCK;
    browserCodeToVkMap["ScrollLock"] = VK_SCROLL;
    browserCodeToVkMap["Pause"] = VK_PAUSE;
    browserCodeToVkMap["PrintScreen"] = VK_SNAPSHOT; 
    browserCodeToVkMap["Semicolon"] = VK_OEM_1; 
    browserCodeToVkMap["Equal"] = VK_OEM_PLUS; 
    browserCodeToVkMap["Comma"] = VK_OEM_COMMA; 
    browserCodeToVkMap["Minus"] = VK_OEM_MINUS; 
    browserCodeToVkMap["Period"] = VK_OEM_PERIOD; 
    browserCodeToVkMap["Slash"] = VK_OEM_2; 
    browserCodeToVkMap["Backquote"] = VK_OEM_3; 
    browserCodeToVkMap["BracketLeft"] = VK_OEM_4; 
    browserCodeToVkMap["Backslash"] = VK_OEM_5; 
    browserCodeToVkMap["BracketRight"] = VK_OEM_6; 
    browserCodeToVkMap["Quote"] = VK_OEM_7; 
}
```

Remote-Share\Agent\src\InputInjector.h:
```cpp
#pragma once
#include <string>
#include <vector>
#include <Windows.h> 
#include <map>       
class InputInjector {
public:
    InputInjector(); 
    void InjectMouseInput(const std::string& inputType, int x, int y, int button = -1, int deltaY = 0);
    void InjectKeyboardInput(const std::string& inputType, const std::string& key, const std::string& code,
                             bool ctrlKey, bool shiftKey, bool altKey, bool metaKey);
private:
    void SendInputEvents(const std::vector<INPUT>& inputs);
    int GetScreenWidth();
    int GetScreenHeight();
    std::map<std::string, WORD> browserCodeToVkMap;
    void InitializeKeyMap();
    WORD GetVirtualKey(const std::string& key);
};
```

Remote-Share\Agent\src\main.cpp:
```cpp
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
const std::string DEFAULT_SESSION_ID = "def_pas"; 
const std::string DEFAULT_SERVER_HOST = "localhost"; 
const std::string SERVER_PORT = "8080";
int main(int argc, char* argv[]) {
    std::string server_host; 
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
            std::cout << "Note: If Node.js server is on another device, use its actual IP address." << std::endl;
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
    std::string server_url = "ws:
    std::cout << "Attempting to connect to WebSocket server at: " << server_url << std::endl;
    WebSocketClient ws_client(server_url);
    InputInjector input_injector;
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
    con->append_header("Sec-WebSocket-Protocol", "remote-share");
    con->append_header("Origin", "http:
    con->add_subprotocol("remote-share");
    m_hdl = con->get_handle();
    m_client.connect(con);
    if (!m_thread.joinable()) {
        m_thread = websocketpp::lib::thread([&]() {
            try {
                m_client.run();
                std::cerr << "WebSocket client run loop ended. Attempting to reconnect..." << std::endl;
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
    m_connected = false;
    std::cerr << "WebSocket connection failed" << std::endl;
    if (m_onCloseHandler) {
        m_onCloseHandler();
    }
    static int reconnectAttempts = 0;
    int delay = std::min(30, ++reconnectAttempts) * 1000; 
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    try {
        connect();
    } catch (const std::exception& e) {
        std::cerr << "Reconnection failed: " << e.what() << std::endl;
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
const http = require('http');
const WebSocket = require('ws');
const url = require('url');
const path = require('path');
const os = require('os'); 
const app = express();
const server = http.createServer(app);
app.use(express.static(path.join(__dirname, '../web')));
const wss = new WebSocket.Server({ server });
const sessions = new Map();
function getLocalIpAddress() {
    const interfaces = os.networkInterfaces();
    for (const name in interfaces) {
        for (const iface of interfaces[name]) {
            if (iface.family === 'IPv4' && !iface.internal) {
                return iface.address;
            }
        }
    }
    return 'localhost'; 
}
const SERVER_PORT = process.env.PORT || 8080;
const SERVER_IP_ADDRESS = getLocalIpAddress(); 
wss.on('connection', (ws, req) => {
    const parsedUrl = url.parse(req.url, true);
    const path = parsedUrl.pathname;
    const sessionId = parsedUrl.query.sessionId;
    if (!sessionId) {
        console.warn('Connection attempt without sessionId. Closing connection.');
        ws.close(1008, 'Session ID required');
        return;
    }
    if (!sessions.has(sessionId)) {
        sessions.set(sessionId, { agent: null, viewers: new Set() });
        console.log(`New session created: ${sessionId}`);
    }
    const currentSession = sessions.get(sessionId);
    if (path === '/agent') {
        if (currentSession.agent && currentSession.agent.readyState === WebSocket.OPEN) {
            console.warn(`Agent already connected for session ${sessionId}. Closing new agent connection.`);
            ws.close(1008, 'Agent already connected to this session');
            return;
        }
        currentSession.agent = ws;
        console.log(`Agent connected to session: ${sessionId}`);
        currentSession.viewers.forEach(viewerWs => {
            if (viewerWs.readyState === WebSocket.OPEN) {
                viewerWs.send(JSON.stringify({ type: 'agent_status', connected: true }));
            }
        });
        ws.on('message', message => {
            currentSession.viewers.forEach(viewerWs => {
                if (viewerWs.readyState === WebSocket.OPEN) {
                    viewerWs.send(message.toString());
                }
            });
        });
        ws.on('close', () => {
            console.log(`Agent disconnected from session: ${sessionId}`);
            currentSession.agent = null;
            currentSession.viewers.forEach(viewerWs => {
                if (viewerWs.readyState === WebSocket.OPEN) {
                    viewerWs.send(JSON.stringify({ type: 'agent_status', connected: false }));
                }
            });
            cleanupSessionIfEmpty(sessionId);
        });
        ws.on('error', error => {
            console.error(`Agent WebSocket Error for session ${sessionId}:`, error);
        });
    } else if (path === '/viewer') {
        currentSession.viewers.add(ws);
        console.log(`Viewer connected to session: ${sessionId}. Total viewers: ${currentSession.viewers.size}`);
        if (currentSession.agent && currentSession.agent.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ type: 'agent_status', connected: true }));
        } else {
            ws.send(JSON.stringify({ type: 'agent_status', connected: false }));
        }
        ws.on('message', message => {
            const inputMessage = message.toString();
            if (currentSession.agent && currentSession.agent.readyState === WebSocket.OPEN) {
                currentSession.agent.send(inputMessage);
            } else {
                console.warn(`Received input for session ${sessionId}, but no agent is connected.`);
            }
        });
        ws.on('close', () => {
            currentSession.viewers.delete(ws);
            console.log(`Viewer disconnected from session: ${sessionId}. Total viewers: ${currentSession.viewers.size}`);
            cleanupSessionIfEmpty(sessionId);
        });
        ws.on('error', error => {
            console.error(`Viewer WebSocket Error for session ${sessionId}:`, error);
        });
    } else {
        console.warn(`Unknown connection path: ${path}. Closing connection.`);
        ws.close(1000, 'Unknown path');
    }
});
function cleanupSessionIfEmpty(sessionId) {
    const session = sessions.get(sessionId);
    if (session && !session.agent && session.viewers.size === 0) {
        sessions.delete(sessionId);
        console.log(`Session ${sessionId} cleaned up (no agent or viewers).`);
    }
}
server.listen(SERVER_PORT, () => {
    console.log(`Server running on http:
    console.log(`Machine IP: ${SERVER_IP_ADDRESS}`);
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
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        /* Import Google Font - Inter */
        @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap');
        body {
            font-family: 'Inter', sans-serif;
            background-color: #f3f4f6; /* Light gray background */
        }
        canvas {
            border: 1px solid #e5e7eb; /* Light border for the canvas */
            background-color: #000; /* Black background for canvas, visible before first frame */
            display: block;
            margin: 0 auto; /* Center the canvas horizontally */
            border-radius: 0.5rem; /* Rounded corners for aesthetics */
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06); /* Subtle shadow */
            /* Ensure canvas fills its container width and maintains aspect ratio for height */
            width: 100%;
            height: auto;
        }
        .container {
            max-width: 1024px; /* Max width for the main content area */
        }
    </style>
</head>
<body class="bg-gray-100 flex items-center justify-center min-h-screen p-4">
    <div class="container mx-auto bg-white p-8 rounded-lg shadow-xl">
        <h1 class="text-3xl font-bold text-center text-gray-800 mb-6">Remote Share Viewer</h1>
        <div class="mb-6 flex flex-col sm:flex-row items-center justify-center space-y-4 sm:space-y-0 sm:space-x-4">
            <input type="text" id="sessionIdInput" placeholder="Enter Session ID" class="p-3 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500 w-full sm:w-64">
            <button id="connectButton" class="bg-blue-600 hover:bg-blue-700 text-white font-semibold py-3 px-6 rounded-md shadow-md transition duration-300 ease-in-out w-full sm:w-auto">
                Connect
            </button>
        </div>
        <div id="statusMessage" class="text-center text-sm font-medium text-gray-600 mb-4">
            Disconnected. Enter a session ID and click Connect.
        </div>
        <div class="relative overflow-hidden rounded-lg">
            <canvas id="remoteScreenCanvas" class="w-full h-auto"></canvas>
            <div id="loadingOverlay" class="absolute inset-0 bg-black bg-opacity-75 flex items-center justify-center text-white text-lg font-semibold rounded-lg hidden">
                Connecting...
            </div>
        </div>
        <div class="mt-6 text-center text-gray-700">
            <p>Once connected, the remote screen will appear above. You can interact with it using your mouse and keyboard.</p>
        </div>
    </div>
    <script src="./script.js"></script>
</body>
</html>
```

Remote-Share\Web\script.js:
```js
let connectButton;
let sessionIdInput;
let statusMessage;
let remoteScreenCanvas;
let loadingOverlay;
let ctx;
let ws = null;
let originalWidth = 0; 
let originalHeight = 0; 
let ipDialog;
let ipInputInDialog;
let ipDialogConnectButton;
let ipDialogStatusMessage;
function showIpInputDialog(callback) {
    if (!ipDialog) {
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
        ipInputInDialog = document.getElementById('ipInputInDialog');
        ipDialogConnectButton = document.getElementById('ipDialogConnectButton');
        ipDialogStatusMessage = document.getElementById('ipDialogStatusMessage');
        ipDialogConnectButton.addEventListener('click', () => {
            const ip = ipInputInDialog.value.trim();
            if (ip) {
                ipDialogStatusMessage.textContent = '';
                callback(ip);
                ipDialog.classList.add('hidden'); 
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
        ipDialog.classList.remove('hidden');
    }
    ipInputInDialog.focus();
}
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
    const wsUrl = `${protocol}
    ws = new WebSocket(wsUrl);
    ws.onopen = () => {
        updateStatus('Connected!', 'success');
        if (loadingOverlay) {
            loadingOverlay.classList.add('hidden');
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
        updateStatus('Disconnected.', 'info');
        if (loadingOverlay) {
            loadingOverlay.classList.add('hidden');
        }
        console.log('WebSocket disconnected.');
        if (serverIp === 'localhost' && sessionIdInput.value.trim() !== '') {
            showIpInputDialog(ip => connectWebSocket(ip)); 
        }
    };
}
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
document.addEventListener('DOMContentLoaded', () => {
    connectButton = document.getElementById('connectButton');
    sessionIdInput = document.getElementById('sessionIdInput');
    statusMessage = document.getElementById('statusMessage');
    remoteScreenCanvas = document.getElementById('remoteScreenCanvas');
    loadingOverlay = document.getElementById('loadingOverlay');
    ctx = remoteScreenCanvas.getContext('2d'); 
    if (connectButton) {
        connectButton.addEventListener('click', () => connectWebSocket('localhost')); 
    }
    if (sessionIdInput) {
        sessionIdInput.addEventListener('keypress', (event) => {
            if (event.key === 'Enter') {
                connectWebSocket('localhost'); 
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
    document.addEventListener('keydown', (e) => {
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
});
```

Remote-Share\Web\style.css:
```css
body {
    margin: 0; 
    overflow: hidden; 
    background-color: #222; 
    font-family: 'Inter', sans-serif; 
}
canvas {
    display: block; 
    background-color: #000; 
    width: 100%; 
    height: 100%; 
}
```

