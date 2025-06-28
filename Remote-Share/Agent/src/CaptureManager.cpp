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
        if (!hdcScreen) {
            std::cerr << "GetDC(NULL) failed. Error: " << GetLastError() << std::endl;
            return pixels;
        }
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
    } else { 
        RECT client_rect;
        if (!GetClientRect(hwnd, &client_rect)) {
            std::cerr << "GetClientRect failed for HWND: " << hwnd << ". Error: " << GetLastError() << std::endl;
            return pixels; 
        }
        
        // First try to get window rect for screen coordinates
        RECT window_rect;
        if (GetWindowRect(hwnd, &window_rect)) {
            width = window_rect.right - window_rect.left;
            height = window_rect.bottom - window_rect.top;
            x_src = window_rect.left;
            y_src = window_rect.top;
        } 
        // Fallback to client area if window rect fails
        else {
            POINT points[2] = {
                {client_rect.left, client_rect.top},
                {client_rect.right, client_rect.bottom}
            };
            if (MapWindowPoints(hwnd, NULL, points, 2)) {
                width = points[1].x - points[0].x;
                height = points[1].y - points[0].y;
                x_src = points[0].x;
                y_src = points[0].y;
            } else {
                std::cerr << "Warning: Failed to map window points for HWND: " << hwnd << ". Using client area with screen position (0,0)" << std::endl;
                width = client_rect.right - client_rect.left;
                height = client_rect.bottom - client_rect.top;
                x_src = 0;
                y_src = 0;
            }
        }
        
        // Get screen DC for window capture
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
        // Full screen capture
        BitBlt(hdcCompatible, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    } else { 
        // Window capture
        BOOL success = PrintWindow(hwnd, hdcCompatible, PW_CLIENTONLY);
        if (!success) {
            std::cerr << "PrintWindow failed for HWND: " << hwnd 
                      << ". Falling back to BitBlt. Error: " << GetLastError() << std::endl;
            BitBlt(hdcCompatible, 0, 0, width, height, hdcScreen, x_src, y_src, SRCCOPY);
        }
    }
    
    // Set up the bitmap info header
    BITMAPINFOHEADER bi = {0};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;  // Negative height to ensure top-down DIB
    bi.biPlanes = 1;
    bi.biBitCount = 24;     // 24 bits per pixel (RGB)
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;
    
    // Calculate the required buffer size
    int bytesPerPixel = 3;  // 24 bits = 3 bytes
    int rowPitch = ((width * bytesPerPixel + 3) / 4) * 4;  // Align to 4 bytes
    pixels.resize(static_cast<size_t>(rowPitch) * height);
    
    // Get the pixel data
    if (!GetDIBits(hdcCompatible, hBitmap, 0, height, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
        std::cerr << "GetDIBits failed! Error: " << GetLastError() << std::endl;
        pixels.clear();
    }
    
    // Clean up
    if (hBitmap) {
        DeleteObject(hBitmap);
    }
    if (hdcCompatible) {
        DeleteDC(hdcCompatible);
    }
    if (hdcScreen) {
        ReleaseDC(NULL, hdcScreen);
    }
    
    return pixels;
}