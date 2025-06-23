#include "WindowEnumerator.h" // Include its own header first

#include <algorithm> // For std::sort
#include <iostream>  // For std::cout, std::cerr, std::cin
#include <limits>    // For std::numeric_limits

// Definition of the static member (initialized to nullptr)
std::vector<WindowInfo>* WindowEnumerator::s_currentWindowsList = nullptr;

// Implementation of the EnumWindowsProc callback function
BOOL CALLBACK WindowEnumerator::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    // Ensure the static list pointer is valid
    if (s_currentWindowsList == nullptr) {
        return FALSE; // Stop enumeration if somehow not set
    }

    // Filter criteria for visible and non-empty title windows
    if (IsWindowVisible(hwnd) && GetWindowTextLength(hwnd) != 0) {
        char buffer[512]; // Buffer for window title
        GetWindowTextA(hwnd, buffer, sizeof(buffer)); // Get title using ANSI function
        std::string title(buffer); // Convert to std::string

        // Filter out common irrelevant windows
        if (title.empty() ||
            title == "Program Manager" ||         // Desktop window
            title == "Default IME" ||             // Input Method Editor
            title == "MSCTFIME UI" ||             // Input Method Editor UI
            title.find("NVIDIA GeForce Overlay") != std::string::npos || // NVIDIA overlay
            title.find("GDI+ Window") != std::string::npos ||         // Often hidden GDI+ windows
            title.find("Qt Message Window") != std::string::npos ||   // Qt internal messages
            title.find("ApplicationManagerWindow") != std::string::npos || // Some system internal windows
            title.find("Cortana") != std::string::npos ||            // Cortana process
            title.find("Windows Input Experience") != std::string::npos || // Modern Windows input
            title.length() < 3 || // Filter out very short titles (often system/hidden)
            (GetConsoleWindow() == hwnd && !IsWindowVisible(hwnd)) // If it's this console window and it's hidden
           )
        {
            return TRUE; // Continue enumeration (skip this window)
        }

        // Add valid window to the list
        s_currentWindowsList->push_back({hwnd, title});
    }
    return TRUE; // Continue enumeration
}

// Implementation for enumerating visible windows
std::vector<WindowInfo> WindowEnumerator::EnumerateVisibleWindows() {
    std::vector<WindowInfo> windows;
    s_currentWindowsList = &windows; // Set the static pointer to our local vector

    EnumWindows(EnumWindowsProc, NULL); // Start enumeration

    s_currentWindowsList = nullptr; // Reset the static pointer after enumeration

    // Sort the list alphabetically by title for better user experience
    std::sort(windows.begin(), windows.end(), [](const WindowInfo& a, const WindowInfo& b) {
        return a.title < b.title;
    });

    return windows;
}

// Implementation for user to select a window from the list
HWND WindowEnumerator::SelectWindowFromList(const std::vector<WindowInfo>& windows) {
    std::cout << "\n--- Select a Window to Share ---" << std::endl;
    std::cout << "0. Full Screen" << std::endl; // Option for full screen

    // List all enumerated windows
    for (size_t i = 0; i < windows.size(); ++i) {
        std::cout << i + 1 << ". " << windows[i].title << std::endl;
    }

    std::cout << "Enter selection (0-" << windows.size() << "): ";
    int choice;

    // Input validation loop
    while (!(std::cin >> choice) || choice < 0 || choice > static_cast<int>(windows.size())) {
        std::cin.clear(); // Clear error flags
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard invalid input
        std::cerr << "Invalid input. Please enter a number between 0 and " << windows.size() << ": ";
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Consume the rest of the line (newline character)

    if (choice == 0) {
        std::cout << "Sharing full screen." << std::endl;
        return NULL; // Return NULL for full screen
    } else {
        HWND selectedHwnd = windows[choice - 1].hwnd; // Get the HWND for the selected window
        std::cout << "Sharing: " << windows[choice - 1].title << std::endl;
        return selectedHwnd;
    }
}