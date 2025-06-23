#pragma once // Prevents multiple inclusions of this header

#include <Windows.h> // For HWND, BOOL, etc.
#include <string>    // For std::string
#include <vector>    // For std::vector

// Define a structure to hold window information
struct WindowInfo {
    HWND hwnd;
    std::string title;
};

// Declare the WindowEnumerator class
class WindowEnumerator {
public:
    // Static methods for enumerating and selecting windows
    static std::vector<WindowInfo> EnumerateVisibleWindows();
    static HWND SelectWindowFromList(const std::vector<WindowInfo>& windows);

private:
    // Callback function used by EnumWindows. Needs to be static.
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

    // Static pointer to the vector that EnumWindowsProc will populate.
    // This is a common pattern for passing data to C-style callbacks.
    static std::vector<WindowInfo>* s_currentWindowsList;
};