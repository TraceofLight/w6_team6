#pragma once
#include "Core/Public/Object.h"
#include <d2d1.h>
#include <dwrite.h>

enum class EStatType : uint8
{
	None = 0,
	FPS = 1 << 0,      // 1
	Memory = 1 << 1,   // 2
	Picking = 1 << 2,  // 4
	Time = 1 << 3,  // 8
	Decal = 1 << 4,
	All = FPS | Memory | Picking | Time | Decal
};

UCLASS()
class UStatOverlay : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UStatOverlay, UObject)

public:
	void Initialize();
	void Release();
	void Render();

	// Stat control methods
	void ShowFPS(bool bShow) { bShow ? EnableStat(EStatType::FPS) : DisableStat(EStatType::FPS); }
	void ShowMemory(bool bShow) { bShow ? EnableStat(EStatType::Memory) : DisableStat(EStatType::Memory); }
	void ShowPicking(bool bShow) { bShow ? EnableStat(EStatType::Picking) : DisableStat(EStatType::Picking); }
	void ShowTime(bool bShow) { bShow ? EnableStat(EStatType::Time) : DisableStat(EStatType::Time); }
	void ShowDecal(bool bShow) { bShow ? EnableStat(EStatType::Decal) : DisableStat(EStatType::Decal); }
	void ShowAll(bool bShow) { SetStatType(bShow ? EStatType::All : EStatType::None); }

	// API to update stats
	void RecordPickingStats(float ElapsedMS);

	// API to update Decal stats
	void ResetDecalFrame();
	void RecordDecalCollection(uint32 Collected, uint32 Visible);
	void RecordDecalDrawCalls(uint32 Calls);
	void RecordDecalTextureStats(uint32 Binds, uint32 Fallbacks);
	void RecordDecalPassMs(float Ms);
	void RecordDecalMaterialStats(uint32 Seen, uint32 Binds);
private:
	void RenderFPS(ID2D1DeviceContext* d2dCtx);
	void RenderMemory(ID2D1DeviceContext* d2dCtx);
	void RenderPicking(ID2D1DeviceContext* d2dCtx);
	void RenderTimeInfo(ID2D1DeviceContext* d2dCtx);
	void RenderText(ID2D1DeviceContext* d2dCtx, const FString& Text, float X, float Y, float R, float G, float B);
	template <typename T>
	inline void SafeRelease(T*& ptr)
	{
		if (ptr)
		{
			ptr->Release();
			ptr = nullptr;
		}
	}

	// FPS Stats
	float CurrentFPS = 0.0f;
	float FrameTime = 0.0f;

	// Picking Stats
	uint32 PickAttempts = 0;
	float LastPickingTimeMs = 0.0f;
	float AccumulatedPickingTimeMs = 0.0f;

	// Rendering position
	float OverlayX = 18.0f;
	float OverlayY = 55.0f;

	uint8 StatMask = static_cast<uint8>(EStatType::None);

	// Helper methods
	std::wstring ToWString(const FString& InStr);
	void EnableStat(EStatType InStatType);
	void DisableStat(EStatType InStatType);
	void SetStatType(EStatType InStatType);
	bool IsStatEnabled(EStatType InStatType) const;

	IDWriteTextFormat* TextFormat = nullptr;
	
	IDWriteFactory* DWriteFactory = nullptr;

	void RenderDecal(ID2D1DeviceContext* d2dCtx);

	struct FDecalStats {
		uint32 Collected = 0;     // 수집된 데칼 수
		uint32 Visible = 0;       // 가시 데칼 수
		uint32 DrawCalls = 0;     // DrawDecalReceiver 호출 총합
		uint32 TextureBinds = 0;  // 텍스처 바인드 횟수
		uint32 TextureFallbacks = 0; // 폴백(컴포넌트 텍스처) 사용 횟수

		uint32 MaterialSeen = 0;    // Mat 포인터가 존재했던 횟수
		uint32 MaterialBinds = 0;   // Mat에서 텍스처가 실제로 바인딩된 횟수

		float LastPassMs = 0.0f;  // 최근 프레임 DecalPass 실행 시간
		float AccumMs = 0.0f;     // 누적
		uint32 Frames = 0;        // 누적 프레임 수
	} DecalStats;

	float DecalMsHistory[10] = { 0.0f };
	uint32 DecalMsCount = 0;   // 누적 개수(최대 10)
	uint32 DecalMsIndex = 0;   // 덮어쓸 위치
	float DecalAvgMs = 0.0f;   // 최근 10개 평균
};
