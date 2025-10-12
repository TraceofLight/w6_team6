#pragma once

#include "Texture/Public/Material.h"
#include "Global/Vector.h"

/**
 * @brief Sprite sheet 기반 SubUV 애니메이션을 제공하는 Material.
 * 기존 UMaterial 인터페이스를 그대로 유지하면서 추가 파라미터만 관리한다.
 */
UCLASS()
class USpriteMaterial : public UMaterial
{
	GENERATED_BODY()
	DECLARE_CLASS(USpriteMaterial, UMaterial)

public:
	USpriteMaterial();
	explicit USpriteMaterial(const FName& InName);
	~USpriteMaterial() override = default;

	// --- SubUV 설정 ---
	void SetSubUVParams(int32 InRows, int32 InCols, float InFPS = 10.0f);
	void SetCurrentFrame(int32 InFrame);
	void SetLooping(bool bInLoop);
	void SetAutoPlay(bool bInAutoPlay);
	void SetFrameRate(float InFPS);

	// --- 애니메이션 제어 ---
	void Play();
	void Pause();
	void Stop();
	void Reset();

	// --- 런타임 업데이트 ---
	void Update(float DeltaTime);

	// --- 셰이더 전달용 데이터 ---
	FVector4 GetSubUVParams() const;

	// --- 상태 조회 ---
	int32 GetRows() const { return SubUVRows; }
	int32 GetCols() const { return SubUVCols; }
	int32 GetCurrentFrame() const { return CurrentFrame; }
	int32 GetTotalFrames() const { return TotalFrames; }
	float GetFrameRate() const { return FrameRate; }
	bool IsLooping() const { return bLooping; }
	bool IsAutoPlay() const { return bAutoPlay; }
	bool IsPlaying() const { return bIsPlaying; }
	bool RequiresTick() const;

private:
	void ClampCurrentFrame();

	int32 SubUVRows = 1;
	int32 SubUVCols = 1;
	int32 TotalFrames = 1;

	int32 CurrentFrame = 0;
	float FrameRate = 10.0f;
	float FrameTimeAccumulator = 0.0f;

	bool bLooping = true;
	bool bAutoPlay = true;
	bool bIsPlaying = false;
};
