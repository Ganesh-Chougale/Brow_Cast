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