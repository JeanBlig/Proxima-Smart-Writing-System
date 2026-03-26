#pragma once 
// Ensures this header is only included once during compilation.
// Prevents "redefinition" errors if multiple files include this header.
#include <windows.h> // Includes the Win32 API declarations (windowing, messages, etc.)

namespace WindowAPI {
    // Forward declaration of the window procedure function.
    // This is the callback that Windows calls whenever an event/message (like resize, close, key press)
    // is sent to your window.
    LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void ShowResponseTextBox();
    
    class window {
    public:
        // Constructor: sets up and creates the actual Win32 window
        window();
    
        // Delete copy constructor — prevents copying the window object.
        // (Makes sense because copying HWND or HINSTANCE would cause problems.)
        window(const window&) = delete;
    
        // Delete copy assignment operator — prevents assigning one window to another.
        window& operator=(const window&) = delete;
    
        // Destructor: cleans up and unregisters the window class
        ~window();
    
        // Processes all pending Windows messages (keyboard, mouse, close, etc.)
        // Returns false if the app should quit (WM_QUIT received).
        bool ProcessMessages();
    
    private:
        // Handle to the application instance (provided by Windows).
        // Used when registering classes and creating windows.
        HINSTANCE m_hInstance = nullptr; // this window belongs to this specific program instance
    
        // Handle to the created window (HWND = "handle to a window").
        // Needed for manipulating or sending messages to the window.
        HWND m_hWnd = nullptr; // this number is how Windows identifies the window we are talking about
    };

}

