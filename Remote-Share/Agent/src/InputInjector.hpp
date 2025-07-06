#pragma once

#include <string>
#include <vector>
#include <Windows.h> // Required for INPUT structure and other Windows API types
#include <map>       // For key code mapping

class InputInjector {
public:
    InputInjector(); // Constructor to initialize key mappings

    // Injects a mouse event
    // @param inputType: "mousemove", "mousedown", "mouseup", "click", "contextmenu", "wheel"
    // @param x: X coordinate (absolute, scaled 0-65535)
    // @param y: Y coordinate (absolute, scaled 0-65535)
    // @param button: For clicks (0:left, 1:middle, 2:right, 3:x1, 4:x2)
    // @param deltaY: For mouse wheel (positive for scroll up, negative for scroll down)
    void InjectMouseInput(const std::string& inputType, int x, int y, int button = -1, int deltaY = 0);

    // Injects a keyboard event
    // @param inputType: "keydown", "keyup"
    // @param key: The 'key' property from browser event (e.g., "a", "Control", "Enter")
    // @param code: The 'code' property from browser event (e.g., "KeyA", "ControlLeft", "NumpadEnter")
    // @param ctrlKey, shiftKey, altKey, metaKey: State of modifier keys from browser event
    void InjectKeyboardInput(const std::string& inputType, const std::string& key, const std::string& code,
                             bool ctrlKey, bool shiftKey, bool altKey, bool metaKey);

private:
    // Helper function to send an array of INPUT structures
    void SendInputEvents(const std::vector<INPUT>& inputs);

    // Get screen dimensions for mouse scaling
    int GetScreenWidth();
    int GetScreenHeight();

    // Map browser key 'code' to Windows Virtual Key Code (VK)
    std::map<std::string, WORD> browserCodeToVkMap;

    // Initialize the mapping from browser 'code' strings to Windows Virtual Key Codes
    void InitializeKeyMap();

    // Helper to get virtual key for standard keys (like 'A', '1', 'Enter')
    WORD GetVirtualKey(const std::string& key);
};
