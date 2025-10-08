#include "AudioVisualizer.h"

AudioVisualizer* g_pVisualizer = nullptr;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        SetTimer(hwnd, 1, 16, NULL);  // ~60 FPS
        return 0;
        
    case WM_TIMER:
        if (g_pVisualizer) {
            g_pVisualizer->Render();
        }
        return 0;
        
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
        
    case WM_SIZE:
        // 处理窗口大小变化
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    
    // 注册窗口类
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"AudioVisualizerClass";
    
    RegisterClassEx(&wc);
    
    // 创建窗口
    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST,  // 置顶窗口，方便游戏时查看
        L"AudioVisualizerClass",
        L"音频可视化 - 左右声道",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 200,
        NULL, NULL, hInstance, NULL
    );
    
    if (!hwnd) {
        CoUninitialize();
        return 0;
    }
    
    // 初始化可视化器
    g_pVisualizer = new AudioVisualizer();
    if (!g_pVisualizer->Initialize(hwnd)) {
        delete g_pVisualizer;
        CoUninitialize();
        return 0;
    }
    
    g_pVisualizer->StartCapture();
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    // 消息循环
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    // 清理
    delete g_pVisualizer;
    CoUninitialize();
    
    return 0;
}