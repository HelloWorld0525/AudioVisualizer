#include <windows.h>
#include <windowsx.h>
#include "AudioVisualizer.h"

AudioVisualizer* g_pVisualizer = nullptr;

const int DEFAULT_WIDTH = 150;
const int DEFAULT_HEIGHT = 300;
const int DEFAULT_RIGHT_MARGIN = 10;
const int DEFAULT_TOP_MARGIN = 50;

// 全局状态：是否处于“激活”模式（Ctrl 按下）
bool g_isActive = false;

void UpdateWindowTransparency(HWND hwnd, bool active) {
    if (active == g_isActive) return;

    LONG exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (active) {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    else {
        exStyle |= WS_EX_TRANSPARENT;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    g_isActive = active;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        RegisterHotKey(hwnd, 1, MOD_CONTROL | MOD_ALT, 'V');
        SetTimer(hwnd, 1, 16, NULL);
        UpdateWindowTransparency(hwnd, false);
        return 0;

    case WM_HOTKEY:
        if (LOWORD(wParam) == 1) {
            BOOL isVisible = IsWindowVisible(hwnd);
            ShowWindow(hwnd, isVisible ? SW_HIDE : SW_SHOW);
        }
        return 0;

    case WM_TIMER: {
        if (g_pVisualizer) {
            g_pVisualizer->Render();
        }

        bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        UpdateWindowTransparency(hwnd, ctrlPressed);
        return 0;
    }

    case WM_NCHITTEST: {
        // 默认穿透（理论上不会走到这里，但安全起见）
        if (!g_isActive) {
            return HTTRANSPARENT;
        }

        // Ctrl 激活：支持拖动 + 调整大小
        POINT pt;
        pt.x = GET_X_LPARAM(lParam);
        pt.y = GET_Y_LPARAM(lParam);
        ScreenToClient(hwnd, &pt);

        RECT rc;
        GetClientRect(hwnd, &rc);

        const int border = 5; // 缩放热区宽度（像素）

        bool onLeft = (pt.x < border);
        bool onRight = (pt.x >= rc.right - border);
        bool onTop = (pt.y < border);
        bool onBottom = (pt.y >= rc.bottom - border);

        // 角落优先
        if (onLeft && onTop)      return HTTOPLEFT;
        if (onRight && onTop)     return HTTOPRIGHT;
        if (onLeft && onBottom)   return HTBOTTOMLEFT;
        if (onRight && onBottom)  return HTBOTTOMRIGHT;
        if (onLeft)               return HTLEFT;
        if (onRight)              return HTRIGHT;
        if (onTop)                return HTTOP;
        if (onBottom)             return HTBOTTOM;

        // 中间区域：拖动
        return HTCAPTION;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        UnregisterHotKey(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"AudioVisualizerClass";

    RegisterClassEx(&wc);

    DWORD style = WS_POPUP;
    DWORD exStyle = WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT;

    HWND hwnd = CreateWindowEx(
        exStyle,
        L"AudioVisualizerClass",
        L"AudioVisualizer",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        DEFAULT_WIDTH, DEFAULT_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        CoUninitialize();
        return 0;
    }

    SetLayeredWindowAttributes(hwnd, 0, 160, LWA_ALPHA);

    g_pVisualizer = new AudioVisualizer();
    if (!g_pVisualizer->Initialize(hwnd)) {
        delete g_pVisualizer;
        CoUninitialize();
        return 0;
    }

    g_pVisualizer->StartCapture();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    RECT workArea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        workArea.right - DEFAULT_WIDTH - DEFAULT_RIGHT_MARGIN,
        workArea.top + DEFAULT_TOP_MARGIN,
        DEFAULT_WIDTH,
        DEFAULT_HEIGHT,
        SWP_SHOWWINDOW | SWP_NOACTIVATE
    );

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    delete g_pVisualizer;
    CoUninitialize();
    return 0; 
}