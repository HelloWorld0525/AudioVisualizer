#include "AudioVisualizer.h"
#include <algorithm>

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

AudioVisualizer::AudioVisualizer() 
    : pD2DFactory(nullptr), pRenderTarget(nullptr),
      pLeftBrush(nullptr), pRightBrush(nullptr),
      pBackgroundBrush(nullptr), pTextBrush(nullptr),
      pEnumerator(nullptr), pDevice(nullptr),
      pAudioClient(nullptr), pCaptureClient(nullptr),
      hwnd(nullptr), pwfx(nullptr),
      leftVolume(0.0f), rightVolume(0.0f), running(false) {
}

AudioVisualizer::~AudioVisualizer() {
    Cleanup();
}

bool AudioVisualizer::Initialize(HWND hwnd) {
    this->hwnd = hwnd;
    
    if (!InitializeD2D()) return false;
    if (!InitializeAudio()) return false;
    
    return true;
}

bool AudioVisualizer::InitializeD2D() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
    if (FAILED(hr)) return false;
    
    RECT rc;
    GetClientRect(hwnd, &rc);
    
    hr = pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(
            hwnd,
            D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)
        ),
        &pRenderTarget
    );
    if (FAILED(hr)) return false;
    
    // 创建画刷
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.8f, 0.0f), &pLeftBrush);
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.8f), &pRightBrush);
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.1f), &pBackgroundBrush);
    pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &pTextBrush);
    
    return true;
}

bool AudioVisualizer::InitializeAudio() {
    HRESULT hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        (void**)&pEnumerator
    );
    if (FAILED(hr)) return false;
    
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) return false;
    
    hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
    if (FAILED(hr)) return false;
    
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) return false;
    
    // 初始化音频客户端（loopback模式捕获系统音频）
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000,  // 1秒缓冲
        0,
        pwfx,
        NULL
    );
    if (FAILED(hr)) return false;
    
    hr = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
    if (FAILED(hr)) return false;
    
    return true;
}

void AudioVisualizer::StartCapture() {
    running = true;
    pAudioClient->Start();
    audioThread = std::thread(&AudioVisualizer::AudioCaptureThread, this);
}

void AudioVisualizer::StopCapture() {
    running = false;
    if (audioThread.joinable()) {
        audioThread.join();
    }
    if (pAudioClient) {
        pAudioClient->Stop();
    }
}

void AudioVisualizer::AudioCaptureThread() {
    while (running) {
        UINT32 packetLength = 0;
        HRESULT hr = pCaptureClient->GetNextPacketSize(&packetLength);
        
        if (FAILED(hr)) break;
        
        while (packetLength != 0) {
            BYTE* pData;
            UINT32 numFramesAvailable;
            DWORD flags;
            
            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            
            if (SUCCEEDED(hr)) {
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                    float* samples = (float*)pData;
                    int channels = pwfx->nChannels;
                    
                    // 分离左右声道并计算音量
                    std::vector<float> leftSamples, rightSamples;
                    
                    for (UINT32 i = 0; i < numFramesAvailable; i++) {
                        leftSamples.push_back(samples[i * channels]);
                        if (channels > 1) {
                            rightSamples.push_back(samples[i * channels + 1]);
                        }
                    }
                    
                    leftVolume = CalculateRMS(leftSamples.data(), leftSamples.size());
                    if (channels > 1) {
                        rightVolume = CalculateRMS(rightSamples.data(), rightSamples.size());
                    } else {
                        rightVolume = leftVolume;
                    }
                }
                
                pCaptureClient->ReleaseBuffer(numFramesAvailable);
            }
            
            hr = pCaptureClient->GetNextPacketSize(&packetLength);
        }
        
        Sleep(10);  // 降低CPU占用
    }
}

float AudioVisualizer::CalculateRMS(const float* samples, int count) {
    if (count == 0) return 0.0f;
    
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        sum += samples[i] * samples[i];
    }
    
    float rms = sqrtf(sum / count);
    
    // 转换为分贝并归一化到0-1范围
    float db = 20.0f * log10f(rms + 0.0001f);
    float normalized = (db + 60.0f) / 60.0f;  // -60dB到0dB映射到0-1
    
    return std::max(0.0f, std::min(1.0f, normalized));
}

void AudioVisualizer::Render() {
    if (!pRenderTarget) return;
    
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
    
    D2D1_SIZE_F size = pRenderTarget->GetSize();
    float width = size.width;
    float height = size.height;
    
    float margin = 20.0f;
    float barHeight = (height - 3 * margin) / 2;
    
    // 绘制左声道 (绿色)
    float leftWidth = leftVolume.load() * (width - 2 * margin);
    D2D1_RECT_F leftRect = D2D1::RectF(
        margin, margin,
        margin + leftWidth, margin + barHeight
    );
    pRenderTarget->FillRectangle(leftRect, pLeftBrush);
    
    // 绘制右声道 (蓝色)
    float rightWidth = rightVolume.load() * (width - 2 * margin);
    D2D1_RECT_F rightRect = D2D1::RectF(
        margin, 2 * margin + barHeight,
        margin + rightWidth, 2 * margin + 2 * barHeight
    );
    pRenderTarget->FillRectangle(rightRect, pRightBrush);
    
    // 绘制边框
    pRenderTarget->DrawRectangle(
        D2D1::RectF(margin, margin, width - margin, margin + barHeight),
        pTextBrush, 2.0f
    );
    pRenderTarget->DrawRectangle(
        D2D1::RectF(margin, 2 * margin + barHeight, width - margin, 2 * margin + 2 * barHeight),
        pTextBrush, 2.0f
    );
    
    pRenderTarget->EndDraw();
}

void AudioVisualizer::Cleanup() {
    StopCapture();
    
    if (pwfx) { CoTaskMemFree(pwfx); pwfx = nullptr; }
    if (pCaptureClient) { pCaptureClient->Release(); pCaptureClient = nullptr; }
    if (pAudioClient) { pAudioClient->Release(); pAudioClient = nullptr; }
    if (pDevice) { pDevice->Release(); pDevice = nullptr; }
    if (pEnumerator) { pEnumerator->Release(); pEnumerator = nullptr; }
    
    if (pLeftBrush) { pLeftBrush->Release(); pLeftBrush = nullptr; }
    if (pRightBrush) { pRightBrush->Release(); pRightBrush = nullptr; }
    if (pBackgroundBrush) { pBackgroundBrush->Release(); pBackgroundBrush = nullptr; }
    if (pTextBrush) { pTextBrush->Release(); pTextBrush = nullptr; }
    if (pRenderTarget) { pRenderTarget->Release(); pRenderTarget = nullptr; }
    if (pD2DFactory) { pD2DFactory->Release(); pD2DFactory = nullptr; }
}