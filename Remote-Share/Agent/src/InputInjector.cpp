#include "InputInjector.h"
#include <iostream>

// Windows API specific includes
#pragma comment(lib, "user32.lib") // Link with user32.lib for SendInput, GetSystemMetrics, etc.

InputInjector::InputInjector() {
    InitializeKeyMap();
}

void InputInjector::SendInputEvents(const std::vector<INPUT>& inputs) {
    if (inputs.empty()) return;

    // Send the input events to the system
    // SendInput returns the number of events successfully injected
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

    // Scale coordinates to the absolute 0-65535 range for SendInput
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    // Ensure screen dimensions are valid to avoid division by zero
    if (screenWidth == 0 || screenHeight == 0) {
        std::cerr << "Error: Screen dimensions are zero. Cannot inject mouse input." << std::endl;
        return;
    }

    // Convert pixels to 0-65535 range
    input.mi.dx = (x * 65535) / screenWidth;
    input.mi.dy = (y * 65535) / screenHeight;

    // Use MOUSEEVENTF_ABSOLUTE and MOUSEEVENTF_VIRTUALDESK for absolute positioning
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

    if (inputType == "mousemove") {
        input.mi.dwFlags |= MOUSEEVENTF_MOVE;
    } else if (inputType == "mousedown") {
        if (button == 0) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;   // Left button
        else if (button == 1) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN; // Middle button
        else if (button == 2) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;  // Right button
        // X buttons can be handled if needed with MOUSEEVENTF_XDOWN
    } else if (inputType == "mouseup") {
        if (button == 0) input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;     // Left button
        else if (button == 1) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;   // Middle button
        else if (button == 2) input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;    // Right button
        // X buttons can be handled if needed with MOUSEEVENTF_XUP
    } else if (inputType == "click") {
        // A click is typically a down and up event. We can send them sequentially.
        // For simplicity, we just inject the up event here, assuming the down was sent.
        // A more robust solution might send both.
        // Or, in the browser, send only mousedown and mouseup, not 'click'.
        // For this implementation, we'll send a down and an up for 'click' type.
        // This is handled by creating two INPUT structures.
        INPUT down_input = input;
        if (button == 0) down_input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        else if (button == 1) down_input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
        else if (button == 2) down_input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;

        INPUT up_input = input;
        if (button == 0) up_input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
        else if (button == 1) up_input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
        else if (button == 2) up_input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;

        SendInputEvents({down_input, up_input});
        return; // Return early as we sent two events
    } else if (inputType == "contextmenu") { // Typically maps to right-click
        input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        INPUT up_input = input;
        up_input.mi.dwFlags = MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        SendInputEvents({input, up_input});
        return;
    } else if (inputType == "wheel") {
        input.mi.dwFlags |= MOUSEEVENTF_WHEEL;
        // WHEEL_DELTA is 120 per "notch" of the wheel. deltaY from browser is usually +/- 100.
        // Adjust scaling if necessary, for now directly using deltaY * WHEEL_DELTA / 100 or similar
        input.mi.mouseData = (DWORD)(deltaY > 0 ? WHEEL_DELTA : -WHEEL_DELTA);
        if (std::abs(deltaY) > 0) { // If there was any scroll amount
             input.mi.mouseData = (DWORD)((deltaY / 100.0) * WHEEL_DELTA); // Scale deltaY if browser sends 100/line
        } else {
             input.mi.mouseData = (DWORD)deltaY; // Use raw delta if browser sends 120/notch directly
        }
    } else {
        std::cerr << "Unknown mouse input type: " << inputType << std::endl;
        return;
    }

    SendInputEvents({input}); // Send the single mouse event
}


void InputInjector::InjectKeyboardInput(const std::string& inputType, const std::string& key, const std::string& code,
                                         bool ctrlKey, bool shiftKey, bool altKey, bool metaKey) {
    
    std::vector<INPUT> inputs;
    INPUT keyboardInput = {0};
    keyboardInput.type = INPUT_KEYBOARD;

    // Determine the Virtual Key Code (VK) or Scan Code
    // Prioritize `code` for physical key, then fallback to `key` for character
    WORD vkCode = 0;
    
    // Attempt to map from browser 'code' (physical key location)
    auto it = browserCodeToVkMap.find(code);
    if (it != browserCodeToVkMap.end()) {
        vkCode = it->second;
        // Use scan code if mapped and preferred, otherwise use virtual key
        keyboardInput.ki.wScan = MapVirtualKey(vkCode, MAPVK_VK_TO_VSC);
        keyboardInput.ki.dwFlags = KEYEVENTF_SCANCODE;
        // Extended key flag for certain keys (e.g., NumpadEnter, Right Ctrl/Alt)
        if (code == "NumpadEnter" || code.find("Right") != std::string::npos || 
            code == "ControlRight" || code == "AltRight" || code == "Insert" || 
            code == "Delete" || code == "Home" || code == "End" || code == "PageUp" || 
            code == "PageDown" || code == "ArrowUp" || code == "ArrowDown" || 
            code == "ArrowLeft" || code == "ArrowRight" || code == "NumpadDivide" || code == "PrintScreen") {
            keyboardInput.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
    } else {
        // Fallback to browser 'key' for character-based mapping (less reliable for physical layout)
        // This is a more complex mapping than just a direct lookup table.
        // For simple alphanumeric keys, `VkKeyScanA` might work, but it's locale-dependent.
        if (key.length() == 1 && std::isalpha(key[0])) { // Simple alpha chars
             vkCode = toupper(key[0]);
        } else if (key.length() == 1 && std::isdigit(key[0])) { // Simple digit chars
             vkCode = key[0];
        } else if (key == "Backspace") vkCode = VK_BACK;
        else if (key == "Tab") vkCode = VK_TAB;
        else if (key == "Enter") vkCode = VK_RETURN;
        else if (key == "Shift") vkCode = VK_SHIFT; // Generic shift, will be specific by code
        else if (key == "Control") vkCode = VK_CONTROL; // Generic ctrl
        else if (key == "Alt") vkCode = VK_MENU; // Generic alt
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
        else if (key == "Meta") vkCode = VK_LWIN; // Windows key (Left) or VK_RWIN for Right
        else if (key.find("F") == 0 && key.length() > 1 && std::isdigit(key[1])) { // F1-F12
            int fNum = std::stoi(key.substr(1));
            if (fNum >= 1 && fNum <= 24) vkCode = VK_F1 + (fNum - 1);
        } else if (key == "NumLock") vkCode = VK_NUMLOCK;
        else if (key == "ScrollLock") vkCode = VK_SCROLL;
        
        // Use virtual key code directly if scan code lookup isn't possible or reliable
        keyboardInput.ki.wVk = vkCode;
        keyboardInput.ki.dwFlags = 0; // Not using scan code flags
        
        // Add extended key flag for certain VKs that also map to extended keys (e.g. Delete, Home, etc)
        if (vkCode == VK_INSERT || vkCode == VK_DELETE || vkCode == VK_HOME || vkCode == VK_END ||
            vkCode == VK_PRIOR || vkCode == VK_NEXT || vkCode == VK_UP || vkCode == VK_DOWN ||
            vkCode == VK_LEFT || vkCode == VK_RIGHT || vkCode == VK_NUMLOCK || vkCode == VK_DIVIDE ||
            vkCode == VK_RETURN && keyboardInput.ki.wScan == MapVirtualKey(VK_RETURN, MAPVK_VK_TO_VSC)) { // Numpad Enter
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

// Maps common browser `code` values to Windows Virtual Key Codes (VKs)
// This is crucial for reliable keyboard input, especially for non-alphanumeric keys and modifiers.
void InputInjector::InitializeKeyMap() {
    // Alphanumeric keys (most common)
    for (char c = 'A'; c <= 'Z'; ++c) {
        browserCodeToVkMap["Key" + std::string(1, c)] = c;
    }
    for (char c = '0'; c <= '9'; ++c) {
        browserCodeToVkMap["Digit" + std::string(1, c)] = c;
    }

    // Function keys
    for (int i = 1; i <= 24; ++i) {
        browserCodeToVkMap["F" + std::to_string(i)] = VK_F1 + (i - 1);
    }

    // Numpad keys
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
    browserCodeToVkMap["NumpadEnter"] = VK_RETURN; // Numpad Enter is also VK_RETURN but needs extended key flag
    
    // Special keys
    browserCodeToVkMap["Backspace"] = VK_BACK;
    browserCodeToVkMap["Tab"] = VK_TAB;
    browserCodeToVkMap["Enter"] = VK_RETURN;
    browserCodeToVkMap["ShiftLeft"] = VK_LSHIFT;
    browserCodeToVkMap["ShiftRight"] = VK_RSHIFT;
    browserCodeToVkMap["ControlLeft"] = VK_LCONTROL;
    browserCodeToVkMap["ControlRight"] = VK_RCONTROL;
    browserCodeToVkMap["AltLeft"] = VK_LMENU; // VK_LMENU is Left Alt
    browserCodeToVkMap["AltRight"] = VK_RMENU; // VK_RMENU is Right Alt
    browserCodeToVkMap["MetaLeft"] = VK_LWIN; // Left Windows key
    browserCodeToVkMap["MetaRight"] = VK_RWIN; // Right Windows key
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
    browserCodeToVkMap["PrintScreen"] = VK_SNAPSHOT; // Print Screen key
    
    // Punctuation and symbol keys (common US layout, may vary by locale)
    browserCodeToVkMap["Semicolon"] = VK_OEM_1; // ; :
    browserCodeToVkMap["Equal"] = VK_OEM_PLUS; // = +
    browserCodeToVkMap["Comma"] = VK_OEM_COMMA; // , <
    browserCodeToVkMap["Minus"] = VK_OEM_MINUS; // - _
    browserCodeToVkMap["Period"] = VK_OEM_PERIOD; // . >
    browserCodeToVkMap["Slash"] = VK_OEM_2; // / ?
    browserCodeToVkMap["Backquote"] = VK_OEM_3; // ` ~
    browserCodeToVkMap["BracketLeft"] = VK_OEM_4; // [ {
    browserCodeToVkMap["Backslash"] = VK_OEM_5; // \ |
    browserCodeToVkMap["BracketRight"] = VK_OEM_6; // ] }
    browserCodeToVkMap["Quote"] = VK_OEM_7; // ' "
}
