#include "Window.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <windows.h>
#include "..\AppLayer\AppLayer.h"

namespace WindowAPI {

    // --- Function declarations ---
    void TakeScreenshot(HWND hwnd, const std::string* filename);
    void CreateMovableTextBox(HWND hwnd);
    void DrawPermanentText(HDC hdcTarget, int x, int y, const std::string& text);
    LRESULT CALLBACK TextBoxProc(HWND hwndEdit, UINT msg, WPARAM wParam, LPARAM lParam);

    // --- UI handles ---
    HWND hPenButton = nullptr;
    HWND hEraserButton = nullptr;
    HWND hScreenshotButton = nullptr;
    HWND hPlaceResponseButton = nullptr;
    HWND hTextBox = nullptr;
    HWND hMainWindow = nullptr;

    // --- Button IDs ---
    #define ID_PEN_BUTTON              1
    #define ID_ERASER_BUTTON           2
    #define ID_SCREENSHOT_BUTTON       3
    #define ID_PLACE_RESPONSE_BUTTON   4

    // --- Custom message ---
    #define WM_COMMIT_TEXT (WM_APP + 1)
    #define WM_SHOW_RESPONSE_TEXTBOX (WM_APP + 2)

    // --- Drawing state ---
    bool isDrawing = false;
    bool isErasing = false;
    COLORREF currentColor = RGB(0, 0, 0);
    int brushSize = 4;
    int eraserSize = 40;

    // --- Mouse tracking ---
    POINT lastPoint = { 0, 0 };

    // --- Back buffer for memory of drawing ---
    HDC hdcMem = nullptr;
    HBITMAP hbmMem = nullptr;
    HBITMAP hbmOld = nullptr;
    int clientWidth = 0;
    int clientHeight = 0;

    // --- Text box dragging state ---
    bool draggingTextBox = false;
    bool resizingTextBox = false;
    POINT dragOffset = { 0, 0 };
    RECT originalTextBoxRect = { 0, 0, 0, 0 };
    POINT resizeStartCursor = { 0, 0 };
    WNDPROC g_OldTextBoxProc = nullptr;
    constexpr int kResizeHandleSize = 16;
    constexpr int kToolbarCaptureExclusionHeight = 48;

    // --- Screenshot function implementation ---
    void TakeScreenshot(HWND hwnd, const std::string* filename)
    {
        if (!hdcMem || !filename) {
            return;
        }

        const int width = clientWidth;
        const int height = clientHeight - kToolbarCaptureExclusionHeight;

        if (width <= 0 || height <= 0) {
            return;
        }

        HDC hdcWindow = GetDC(hwnd);
        HDC hdcCapture = CreateCompatibleDC(hdcWindow);

        HBITMAP hbmScreen = CreateCompatibleBitmap(hdcWindow, width, height);
        HGDIOBJ oldBitmap = SelectObject(hdcCapture, hbmScreen);

        // Save only the drawable canvas area. This excludes the toolbar band at
        // the top and avoids child controls leaking into OCR/prompt input.
        BitBlt(
            hdcCapture,
            0,
            0,
            width,
            height,
            hdcMem,
            0,
            kToolbarCaptureExclusionHeight,
            SRCCOPY
        );

        BITMAP bmp;
        GetObject(hbmScreen, sizeof(BITMAP), &bmp);

        BITMAPFILEHEADER bmfHeader = { 0 };
        BITMAPINFOHEADER bi = { 0 };

        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmp.bmWidth;
        bi.biHeight = -bmp.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        DWORD dwBmpSize = bmp.bmWidth * 4 * bmp.bmHeight;
        std::vector<BYTE> pixels(dwBmpSize);

        GetDIBits(hdcCapture, hbmScreen, 0, bmp.bmHeight, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

        DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        bmfHeader.bfType = 0x4D42;
        bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bmfHeader.bfSize = dwSizeofDIB;

        std::ofstream file(filename->c_str(), std::ios::out | std::ios::binary);
        file.write((char*)&bmfHeader, sizeof(bmfHeader));
        file.write((char*)&bi, sizeof(bi));
        file.write((char*)pixels.data(), dwBmpSize);
        file.close();

        SelectObject(hdcCapture, oldBitmap);
        DeleteObject(hbmScreen);
        DeleteDC(hdcCapture);
        ReleaseDC(hwnd, hdcWindow);
    }

    // --- Draw permanent text onto the memory canvas ---
    void DrawPermanentText(HDC hdcTarget, int x, int y, const std::string& text)
    {
        if (!hdcTarget || text.empty()) {
            return;
        }

        SetBkMode(hdcTarget, TRANSPARENT);
        SetTextColor(hdcTarget, RGB(0, 0, 0));

        RECT rect = { x, y, x + 500, y + 200 };
        DrawTextA(
            hdcTarget,
            text.c_str(),
            -1,
            &rect,
            DT_LEFT | DT_TOP | DT_WORDBREAK
        );
    }

    // --- Create textbox prefilled with AppLayer::LLM_Response ---
    void CreateMovableTextBox(HWND hwnd)
    {
        if (hTextBox != nullptr) {
            SetFocus(hTextBox);
            return;
        }

        std::string responseText;
        {
            std::lock_guard<std::mutex> lock(AppLayer::StateMutex);
            responseText = AppLayer::LLM_Response;
        }

        hTextBox = CreateWindowExA(
            0,
            "EDIT",
            "",
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
            100, 100, 320, 120,
            hwnd,
            nullptr,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            nullptr
        );

        if (!hTextBox) {
            return;
        }

        SetWindowTextA(hTextBox, responseText.c_str());
        SetFocus(hTextBox);

        g_OldTextBoxProc = (WNDPROC)SetWindowLongPtr(
            hTextBox,
            GWLP_WNDPROC,
            (LONG_PTR)TextBoxProc
        );
    }

    void ShowResponseTextBox()
    {
        if (hMainWindow != nullptr) {
            PostMessage(hMainWindow, WM_SHOW_RESPONSE_TEXTBOX, 0, 0);
        }
    }

    // --- Subclass proc for textbox ---
    LRESULT CALLBACK TextBoxProc(HWND hwndEdit, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg) {
        case WM_KEYDOWN:
            return 0;

        case WM_CHAR:
            return 0;

        case WM_SETCURSOR:
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwndEdit, &pt);

            RECT rcClient;
            GetClientRect(hwndEdit, &rcClient);

            if (pt.x >= rcClient.right - kResizeHandleSize && pt.y >= rcClient.bottom - kResizeHandleSize) {
                SetCursor(LoadCursor(nullptr, IDC_SIZENWSE));
                return TRUE;
            }

            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            return TRUE;
        }

        case WM_LBUTTONDOWN:
        {
            SetCapture(hwndEdit);

            RECT rcClient;
            GetClientRect(hwndEdit, &rcClient);

            const int mouseX = LOWORD(lParam);
            const int mouseY = HIWORD(lParam);

            if (mouseX >= rcClient.right - kResizeHandleSize && mouseY >= rcClient.bottom - kResizeHandleSize) {
                resizingTextBox = true;
                GetWindowRect(hwndEdit, &originalTextBoxRect);
                POINT topLeft = { originalTextBoxRect.left, originalTextBoxRect.top };
                POINT bottomRight = { originalTextBoxRect.right, originalTextBoxRect.bottom };
                ScreenToClient(GetParent(hwndEdit), &topLeft);
                ScreenToClient(GetParent(hwndEdit), &bottomRight);
                originalTextBoxRect.left = topLeft.x;
                originalTextBoxRect.top = topLeft.y;
                originalTextBoxRect.right = bottomRight.x;
                originalTextBoxRect.bottom = bottomRight.y;
                GetCursorPos(&resizeStartCursor);
                return 0;
            }

            draggingTextBox = true;
            dragOffset.x = mouseX;
            dragOffset.y = mouseY;
            return 0;
        }

        case WM_MOUSEMOVE:
        {
            if (resizingTextBox) {
                POINT pt;
                GetCursorPos(&pt);

                const int deltaX = pt.x - resizeStartCursor.x;
                const int deltaY = pt.y - resizeStartCursor.y;

                const int newWidth = std::max<int>(160, static_cast<int>((originalTextBoxRect.right - originalTextBoxRect.left) + deltaX));
                const int newHeight = std::max<int>(60, static_cast<int>((originalTextBoxRect.bottom - originalTextBoxRect.top) + deltaY));

                MoveWindow(
                    hwndEdit,
                    originalTextBoxRect.left,
                    originalTextBoxRect.top,
                    newWidth,
                    newHeight,
                    TRUE
                );
                return 0;
            }

            if (draggingTextBox) {
                POINT pt;
                GetCursorPos(&pt);

                HWND hwndParent = GetParent(hwndEdit);
                ScreenToClient(hwndParent, &pt);

                RECT rc;
                GetWindowRect(hwndEdit, &rc);
                int width = rc.right - rc.left;
                int height = rc.bottom - rc.top;

                int newX = pt.x - dragOffset.x;
                int newY = pt.y - dragOffset.y;

                MoveWindow(hwndEdit, newX, newY, width, height, TRUE);
                return 0;
            }
            break;
        }

        case WM_LBUTTONUP:
        {
            draggingTextBox = false;
            resizingTextBox = false;
            ReleaseCapture();
            return 0;
        }

        case WM_NCDESTROY:
        {
            if (hwndEdit == hTextBox) {
                hTextBox = nullptr;
                draggingTextBox = false;
                resizingTextBox = false;
            }
            break;
        }
        }

        return CallWindowProc(g_OldTextBoxProc, hwndEdit, msg, wParam, lParam);
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg) {

        case WM_CREATE:
        {
            hMainWindow = hwnd;

            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            int buttonWidth = 100;
            int buttonHeight = 30;
            int padding = 5;

            hPenButton = CreateWindowW(
                L"BUTTON",
                L"Pen",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                rcClient.right - (buttonWidth * 3 + padding * 4),
                padding,
                buttonWidth,
                buttonHeight,
                hwnd,
                (HMENU)ID_PEN_BUTTON,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            hEraserButton = CreateWindowW(
                L"BUTTON",
                L"Eraser",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                rcClient.right - (buttonWidth * 2 + padding * 3),
                padding,
                buttonWidth,
                buttonHeight,
                hwnd,
                (HMENU)ID_ERASER_BUTTON,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            hScreenshotButton = CreateWindowW(
                L"BUTTON",
                L"Prompt",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                10, 10, 100, 30,
                hwnd,
                (HMENU)ID_SCREENSHOT_BUTTON,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            hPlaceResponseButton = CreateWindowW(
                L"BUTTON",
                L"Place Response",
                WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                120, 10, 130, 30,
                hwnd,
                (HMENU)ID_PLACE_RESPONSE_BUTTON,
                (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
                NULL
            );

            GetClientRect(hwnd, &rcClient);
            clientWidth = rcClient.right - rcClient.left;
            clientHeight = rcClient.bottom - rcClient.top;

            HDC hdcWindow = GetDC(hwnd);
            hdcMem = CreateCompatibleDC(hdcWindow);
            hbmMem = CreateCompatibleBitmap(hdcWindow, clientWidth, clientHeight);
            hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

            HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdcMem, &rcClient, hBrush);
            DeleteObject(hBrush);

            ReleaseDC(hwnd, hdcWindow);
        }
        return 0;

        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
            case ID_PEN_BUTTON:
                isErasing = false;
                SendMessage(hPenButton, BM_SETSTATE, TRUE, 0);
                SendMessage(hEraserButton, BM_SETSTATE, FALSE, 0);
                break;

            case ID_ERASER_BUTTON:
                isErasing = true;
                SendMessage(hEraserButton, BM_SETSTATE, TRUE, 0);
                SendMessage(hPenButton, BM_SETSTATE, FALSE, 0);
                break;

            case ID_SCREENSHOT_BUTTON:
                if (!AppLayer::PromptInProgress.load()) {
                    TakeScreenshot(hwnd, &AppLayer::bmpPath);
                    AppLayer::Prompting = true;
                }
                break;

            case ID_PLACE_RESPONSE_BUTTON:
                if (hTextBox != nullptr) {
                    PostMessage(hwnd, WM_COMMIT_TEXT, 0, 0);
                }
                break;
            }
            return 0;
        }

        case WM_SHOW_RESPONSE_TEXTBOX:
            CreateMovableTextBox(hwnd);
            return 0;

        case WM_COMMIT_TEXT:
        {
            if (hTextBox) {
                char buffer[4096];
                GetWindowTextA(hTextBox, buffer, sizeof(buffer));

                RECT rc;
                GetWindowRect(hTextBox, &rc);

                POINT pt = { rc.left, rc.top };
                ScreenToClient(hwnd, &pt);

                DrawPermanentText(hdcMem, pt.x, pt.y, std::string(buffer));

                DestroyWindow(hTextBox);
                hTextBox = nullptr;

                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN:
            if (hTextBox != nullptr) {
                // Let textbox handle its own dragging when clicked directly.
                // Otherwise normal drawing still works outside the textbox.
            }
            isDrawing = true;
            lastPoint.x = LOWORD(lParam);
            lastPoint.y = HIWORD(lParam);
            SetCapture(hwnd);
            return 0;

        case WM_MOUSEMOVE:
            if (isDrawing) {
                COLORREF drawColor = isErasing ? RGB(255, 255, 255) : currentColor;
                HPEN pen = CreatePen(PS_SOLID, isErasing ? eraserSize : brushSize, drawColor);
                HGDIOBJ oldPen = SelectObject(hdcMem, pen);

                MoveToEx(hdcMem, lastPoint.x, lastPoint.y, NULL);
                LineTo(hdcMem, LOWORD(lParam), HIWORD(lParam));

                SelectObject(hdcMem, oldPen);
                DeleteObject(pen);

                InvalidateRect(hwnd, NULL, FALSE);

                lastPoint.x = LOWORD(lParam);
                lastPoint.y = HIWORD(lParam);
            }
            return 0;

        case WM_LBUTTONUP:
            isDrawing = false;
            ReleaseCapture();
            return 0;

        case WM_SIZE:
        {
            int newWidth = LOWORD(lParam);
            int newHeight = HIWORD(lParam);

            if (newWidth <= 0 || newHeight <= 0 || !hdcMem) {
                return 0;
            }

            HDC hdcWindow = GetDC(hwnd);
            HDC hdcNewMem = CreateCompatibleDC(hdcWindow);
            HBITMAP hbmNew = CreateCompatibleBitmap(hdcWindow, newWidth, newHeight);
            HBITMAP hbmNewOld = (HBITMAP)SelectObject(hdcNewMem, hbmNew);

            RECT rcNew = { 0, 0, newWidth, newHeight };
            HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdcNewMem, &rcNew, hBrush);
            DeleteObject(hBrush);

            BitBlt(
                hdcNewMem,
                0, 0,
                (newWidth < clientWidth ? newWidth : clientWidth),
                (newHeight < clientHeight ? newHeight : clientHeight),
                hdcMem,
                0, 0,
                SRCCOPY
            );

            SelectObject(hdcMem, hbmOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);

            hdcMem = hdcNewMem;
            hbmMem = hbmNew;
            hbmOld = hbmNewOld;
            clientWidth = newWidth;
            clientHeight = newHeight;

            ReleaseDC(hwnd, hdcWindow);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            BitBlt(hdc, 0, 0, clientWidth, clientHeight, hdcMem, 0, 0, SRCCOPY);

            EndPaint(hwnd, &ps);
        }
        return 0;

        case WM_DESTROY:
            hMainWindow = nullptr;

            if (hTextBox) {
                DestroyWindow(hTextBox);
                hTextBox = nullptr;
            }

            if (hdcMem) {
                SelectObject(hdcMem, hbmOld);
                DeleteObject(hbmMem);
                DeleteDC(hdcMem);
                hdcMem = nullptr;
                hbmMem = nullptr;
                hbmOld = nullptr;
            }

            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }

    window::window()
        : m_hInstance(GetModuleHandle(nullptr))
    {
        const wchar_t* CLASS_NAME = L"Sample Window Class";

        WNDCLASSW wc = {};
        wc.lpfnWndProc   = WindowProc;
        wc.hInstance     = m_hInstance;
        wc.lpszClassName = CLASS_NAME;
        wc.hIcon         = LoadIconW(nullptr, MAKEINTRESOURCEW(IDI_APPLICATION));
        wc.hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

        if (!RegisterClassW(&wc)) {
            DWORD err = GetLastError();
            std::wcerr << L"RegisterClassW failed, error: " << err << std::endl;
            throw std::runtime_error("RegisterClassW failed");
        }

        DWORD style = WS_OVERLAPPEDWINDOW;

        int width  = 640;
        int height = 480;

        RECT rect = { 250, 250, 250 + width, 250 + height };
        AdjustWindowRect(&rect, style, FALSE);

        m_hWnd = CreateWindowExW(
            0,
            CLASS_NAME,
            L"Proxima Whiteboard",
            style,
            rect.left,
            rect.top,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            m_hInstance,
            nullptr
        );

        if (!m_hWnd) {
            DWORD err = GetLastError();
            std::wcerr << L"CreateWindowExW failed, error: " << err << std::endl;
            UnregisterClassW(CLASS_NAME, m_hInstance);
            throw std::runtime_error("CreateWindowExW failed");
        }

        ShowWindow(m_hWnd, SW_SHOW);
        UpdateWindow(m_hWnd);
        std::cout << "Window successfully created!" << std::endl;
    }

    window::~window()
    {
        const wchar_t* CLASS_NAME = L"Sample Window Class";

        if (m_hWnd) {
            DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
        }

        UnregisterClassW(CLASS_NAME, m_hInstance);
    }

    bool window::ProcessMessages()
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return true;
    }

}
