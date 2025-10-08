#pragma once
#include <windows.h>
#include <d2d1.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <comdef.h>
#include <thread>
#include <atomic>
#include <cmath>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

class AudioVisualizer {
private:
    // Direct2D资源
    ID2D1Factory* pD2DFactory;
    ID2D1HwndRenderTarget* pRenderTarget;
    ID2D1SolidColorBrush* pLeftBrush;
    ID2D1SolidColorBrush* pRightBrush;
    ID2D1SolidColorBrush* pBackgroundBrush;
    ID2D1SolidColorBrush* pTextBrush;
    
    // WASAPI资源
    IMMDeviceEnumerator* pEnumerator;
    IMMDevice* pDevice;
    IAudioClient* pAudioClient;
    IAudioCaptureClient* pCaptureClient;
    
    // 窗口句柄
    HWND hwnd;
    
    // 音频数据
    std::atomic<float> leftVolume;
    std::atomic<float> rightVolume;
    
    // 控制线程
    std::thread audioThread;
    std::atomic<bool> running;
    
    // 音频格式
    WAVEFORMATEX* pwfx;
    
public:
    AudioVisualizer();
    ~AudioVisualizer();
    
    bool Initialize(HWND hwnd);
    void Cleanup();
    void Render();
    void StartCapture();
    void StopCapture();
    
private:
    bool InitializeD2D();
    bool InitializeAudio();
    void AudioCaptureThread();
    float CalculateRMS(const float* samples, int count);
};