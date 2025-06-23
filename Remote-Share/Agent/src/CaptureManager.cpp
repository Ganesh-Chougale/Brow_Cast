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