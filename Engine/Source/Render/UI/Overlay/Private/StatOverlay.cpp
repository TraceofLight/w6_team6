#include "pch.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Global/Types.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Global/Memory.h"
#include "Render/Renderer/Public/Renderer.h"

IMPLEMENT_SINGLETON_CLASS_BASE(UStatOverlay)

UStatOverlay::UStatOverlay() {}
UStatOverlay::~UStatOverlay() = default;

void UStatOverlay::Initialize()
{
    auto* DeviceResources = URenderer::GetInstance().GetDeviceResources();
    DWriteFactory = DeviceResources->GetDWriteFactory();

    if (DWriteFactory)
    {
        DWriteFactory->CreateTextFormat(
            L"Consolas",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            15.0f,
            L"en-us",
            &TextFormat
        );
    }
}

void UStatOverlay::Release()
{
    SafeRelease(TextFormat);

    DWriteFactory = nullptr;
}

void UStatOverlay::Render()
{
    TIME_PROFILE(StatDrawn);

    auto* DeviceResources = URenderer::GetInstance().GetDeviceResources();
    IDXGISwapChain* SwapChain = DeviceResources->GetSwapChain();
    ID3D11Device* D3DDevice = DeviceResources->GetDevice();

    ID2D1Factory1* D2DFactory = nullptr;
    D2D1_FACTORY_OPTIONS opts{};
#ifdef _DEBUG
    opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &opts, (void**)&D2DFactory)))
        return;

    IDXGISurface* Surface = nullptr;
    SwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&Surface);

    IDXGIDevice* DXGIDevice = nullptr;
    D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DXGIDevice);

    ID2D1Device* D2DDevice = nullptr;
    D2DFactory->CreateDevice(DXGIDevice, &D2DDevice);

    ID2D1DeviceContext* D2DCtx = nullptr;
    D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2DCtx);

    D2D1_BITMAP_PROPERTIES1 BmpProps = {};
    BmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    BmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    BmpProps.dpiX = 96.0f;
    BmpProps.dpiY = 96.0f;
    BmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    ID2D1Bitmap1* TargetBmp = nullptr;
    D2DCtx->CreateBitmapFromDxgiSurface(Surface, &BmpProps, &TargetBmp);

    D2DCtx->SetTarget(TargetBmp);
    D2DCtx->BeginDraw();

    if (IsStatEnabled(EStatType::FPS))     RenderFPS(D2DCtx);
    if (IsStatEnabled(EStatType::Memory))  RenderMemory(D2DCtx);
    if (IsStatEnabled(EStatType::Picking)) RenderPicking(D2DCtx);
    if (IsStatEnabled(EStatType::Time))    RenderTimeInfo(D2DCtx);
    if (IsStatEnabled(EStatType::Decal))  RenderDecal(D2DCtx);

    D2DCtx->EndDraw();
    D2DCtx->SetTarget(nullptr);

    SafeRelease(TargetBmp);
    SafeRelease(D2DCtx);
    SafeRelease(D2DDevice);
    SafeRelease(DXGIDevice);
    SafeRelease(Surface);
    SafeRelease(D2DFactory);
}

void UStatOverlay::RenderFPS(ID2D1DeviceContext* D2DCtx)
{
    auto& timeManager = UTimeManager::GetInstance();
    CurrentFPS = timeManager.GetFPS();
    FrameTime = timeManager.GetDeltaTime() * 1000;

    char buf[64];
    sprintf_s(buf, sizeof(buf), "FPS: %.1f (%.2f ms)", CurrentFPS, FrameTime);
    FString text = buf;

    float r = 0.5f, g = 1.0f, b = 0.5f;
    if (CurrentFPS < 30.0f) { r = 1.0f; g = 0.0f; b = 0.0f; }
    else if (CurrentFPS < 60.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

    RenderText(D2DCtx, text, OverlayX, OverlayY, r, g, b);
}

void UStatOverlay::RenderMemory(ID2D1DeviceContext* d2dCtx)
{
    float MemoryMB = static_cast<float>(TotalAllocationBytes) / (1024.0f * 1024.0f);

    char Buf[64];
    sprintf_s(Buf, sizeof(Buf), "Memory: %.1f MB (%u objects)", MemoryMB, TotalAllocationCount);
    FString text = Buf;

    float OffsetY = IsStatEnabled(EStatType::FPS) ? 20.0f : 0.0f;
    RenderText(d2dCtx, text, OverlayX, OverlayY + OffsetY, 1.0f, 1.0f, 0.0f);
}

void UStatOverlay::RenderPicking(ID2D1DeviceContext* D2DCtx)
{
    float AvgMs = PickAttempts > 0 ? AccumulatedPickingTimeMs / PickAttempts : 0.0f;

    char Buf[128];
    sprintf_s(Buf, sizeof(Buf), "Picking Time %.2f ms (Attempts %u, Accum %.2f ms, Avg %.2f ms)",
        LastPickingTimeMs, PickAttempts, AccumulatedPickingTimeMs, AvgMs);
    FString Text = Buf;

    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Memory)) OffsetY += 20.0f;

    float r = 0.0f, g = 1.0f, b = 0.8f;
    if (LastPickingTimeMs > 5.0f) { r = 1.0f; g = 0.0f; b = 0.0f; }
    else if (LastPickingTimeMs > 1.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

    RenderText(D2DCtx, Text, OverlayX, OverlayY + OffsetY, r, g, b);
}

void UStatOverlay::RenderTimeInfo(ID2D1DeviceContext* D2DCtx)
{
    const TArray<FString> ProfileKeys = FScopeCycleCounter::GetTimeProfileKeys();

    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Memory)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Picking)) OffsetY += 20.0f;

    float CurrentY = OverlayY + OffsetY;
    const float LineHeight = 20.0f;

    for (const FString& Key : ProfileKeys)
    {
        const FTimeProfile& Profile = FScopeCycleCounter::GetTimeProfile(Key);

        char buf[128];
        sprintf_s(buf, sizeof(buf), "%s: %.2f ms", Key.c_str(), Profile.Milliseconds);
        FString text = buf;

        float r = 0.8f, g = 0.8f, b = 0.8f;
        if (Profile.Milliseconds > 1.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

        RenderText(D2DCtx, text, OverlayX, CurrentY, r, g, b);
        CurrentY += LineHeight;
    }
}

void UStatOverlay::RenderText(ID2D1DeviceContext* D2DCtx, const FString& Text, float x, float y, float r, float g, float b)
{
    if (!D2DCtx || Text.empty() || !TextFormat) return;

    std::wstring wText = ToWString(Text);

    ID2D1SolidColorBrush* Brush = nullptr;
    if (FAILED(D2DCtx->CreateSolidColorBrush(D2D1::ColorF(r, g, b), &Brush)))
        return;

    D2D1_RECT_F rect = D2D1::RectF(x, y, x + 800.0f, y + 20.0f);
    D2DCtx->DrawTextW(
        wText.c_str(),
        static_cast<UINT32>(wText.length()),
        TextFormat,
        &rect,
        Brush
    );

    SafeRelease(Brush);
}

std::wstring UStatOverlay::ToWString(const FString& InStr)
{
    if (InStr.empty()) return std::wstring();

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), (int)InStr.size(), NULL, 0);
    std::wstring wStr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, InStr.c_str(), (int)InStr.size(), &wStr[0], sizeNeeded);
    return wStr;
}

void UStatOverlay::EnableStat(EStatType type) { StatMask |= static_cast<uint8>(type); }
void UStatOverlay::DisableStat(EStatType type) { StatMask &= ~static_cast<uint8>(type); }
void UStatOverlay::SetStatType(EStatType type) { StatMask = static_cast<uint8>(type); }
bool UStatOverlay::IsStatEnabled(EStatType type) const { return (StatMask & static_cast<uint8>(type)) != 0; }

void UStatOverlay::RecordPickingStats(float elapsedMs)
{
    ++PickAttempts;
    LastPickingTimeMs = elapsedMs;
    AccumulatedPickingTimeMs += elapsedMs;
}

void UStatOverlay::RenderDecal(ID2D1DeviceContext* ctx)
{
    // 위 섹션들과 동일하게 Offset 누적
    float OffsetY = 0.0f;
    if (IsStatEnabled(EStatType::FPS))    OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Memory)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Picking)) OffsetY += 20.0f;
    if (IsStatEnabled(EStatType::Time))   OffsetY += 20.0f;

    const float Y = OverlayY + OffsetY;
    const float LineH = 20.0f;

    // 1줄: 데칼 카운트 정보만
    {
        char Line[96];
        sprintf_s(Line, sizeof(Line), "Decal: Collected %u, Visible %u, Drawcalls %u",
            DecalStats.Collected, DecalStats.Visible, DecalStats.DrawCalls);
        RenderText(ctx, Line, OverlayX, Y, 0.75f, 0.85f, 1.0f);
    }

    // 2줄: 텍스처/머티리얼(바인딩/폴백)
    {
        char Line[128];
        // MaterialSeen을 함께 보이고 싶으면 MatSeen도 노출
        sprintf_s(Line, sizeof(Line), "Tex: Bind %u (Fallback %u), Mat: Bind %u / Seen %u",
            DecalStats.TextureBinds, DecalStats.TextureFallbacks,
            DecalStats.MaterialBinds, DecalStats.MaterialSeen);
        RenderText(ctx, Line, OverlayX, Y + LineH, 0.6f, 0.9f, 1.0f);
    }
    {
        // 3줄: 패스 시간(Last/Avg)
        const float avgMs = DecalAvgMs;
        char Line[96];
        sprintf_s(Line, sizeof(Line), "LastPass: %.2f ms (Recent 10 Pass Avg %.2f)", DecalStats.LastPassMs, avgMs);

        float r = 0.8f, g = 0.8f, b = 0.8f;
        if (DecalStats.LastPassMs > 5.0f) { r = 1.0f; g = 0.0f; b = 0.0f; }
        else if (DecalStats.LastPassMs > 2.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }

        RenderText(ctx, Line, OverlayX, Y + LineH * 2, r, g, b);
    }
}

void UStatOverlay::ResetDecalFrame()
{
    DecalStats.Collected = DecalStats.Visible = 0;
    DecalStats.DrawCalls = 0;
    DecalStats.TextureBinds = DecalStats.TextureFallbacks = 0;
    DecalStats.MaterialSeen = DecalStats.MaterialBinds = 0;
    DecalStats.LastPassMs = 0.0f;
    // 누적은 유지
}

void UStatOverlay::RecordDecalCollection(uint32 Collected, uint32 Visible)
{
    DecalStats.Collected += Collected;
    DecalStats.Visible += Visible;
}

void UStatOverlay::RecordDecalDrawCalls(uint32 Calls)
{
    DecalStats.DrawCalls += Calls;
}

void UStatOverlay::RecordDecalTextureStats(uint32 Binds, uint32 Fallbacks)
{
    DecalStats.TextureBinds += Binds;
    DecalStats.TextureFallbacks += Fallbacks;
}

void UStatOverlay::RecordDecalPassMs(float Ms)
{
    DecalStats.LastPassMs = Ms;

    // 1) 링 버퍼 삽입
    if (DecalMsCount < 10)
    {
        DecalMsHistory[DecalMsCount++] = Ms;
    }
    else
    {
        DecalMsHistory[DecalMsIndex] = Ms;
        DecalMsIndex = (DecalMsIndex + 1) % 10;
    }

    // 2) 평균 재계산
    float sum = 0.0f;
    for (uint32 i = 0; i < DecalMsCount; ++i) sum += DecalMsHistory[i];
    DecalAvgMs = (DecalMsCount > 0) ? (sum / DecalMsCount) : 0.0f;
}
void UStatOverlay::RecordDecalMaterialStats(uint32 Seen, uint32 Binds)
{
    DecalStats.MaterialSeen += Seen;
    DecalStats.MaterialBinds += Binds;
}