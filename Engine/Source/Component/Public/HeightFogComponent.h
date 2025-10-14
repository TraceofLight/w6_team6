#pragma once
#include "Component/Public/SceneComponent.h"

/**
 * @brief Exponential Height Fog Component
 *
 * 언리얼 엔진의 ExponentialHeightFog를 참조한 구현
 * 고도에 따라 밀도가 변하는 지수 함수 기반 안개 효과
 *
 * @note Z-up Left-Handed 좌표계 사용
 * - Z축: 위아래 (Up/Height): 안개 높이 계산에 사용, 나머지 축은 사용하지 않음
 * 
 * @param FogDensity 안개의 전체 밀도 (0.0 ~ 1.0)
 * @param FogHeightFalloff: 고도(Z축)에 따른 밀도 감소율 (값이 클수록 급격히 감소)
 * @param StartDistance: 안개 시작 거리 (카메라로부터)
 * @param FogCutoffDistance: 안개 최대 영향 거리
 * @param FogMaxOpacity: 안개의 최대 불투명도 (0.0 ~ 1.0)
 * @param FogInscatteringColor: 안개 색상 (RGB + Alpha)
 */
UCLASS()
class UHeightFogComponent :
    public USceneComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UHeightFogComponent, USceneComponent)

public:
    UHeightFogComponent();
    ~UHeightFogComponent() override;

    void TickComponent() override;

    UObject* Duplicate() override;

    // Fog Properties Setters
    void SetFogDensity(float InDensity) { FogDensity = clamp(InDensity, 0.0f, 10.0f); }
    void SetFogHeightFalloff(float InFalloff) { FogHeightFalloff = max(0.001f, InFalloff); }
    void SetStartDistance(float InDistance) { StartDistance = max(0.0f, InDistance); }
    void SetFogCutoffDistance(float InDistance) { FogCutoffDistance = max(0.0f, InDistance); }
    void SetFogMaxOpacity(float InOpacity) { FogMaxOpacity = clamp(InOpacity, 0.0f, 1.0f); }
    void SetFogInscatteringColor(const FVector4& InColor) { FogInscatteringColor = InColor; }
    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

    // Fog Properties Getters
    float GetFogDensity() const { return FogDensity; }
    float GetFogHeightFalloff() const { return FogHeightFalloff; }
    float GetStartDistance() const { return StartDistance; }
    float GetFogCutoffDistance() const { return FogCutoffDistance; }
    float GetFogMaxOpacity() const { return FogMaxOpacity; }
    FVector4 GetFogInscatteringColor() const { return FogInscatteringColor; }
    bool IsEnabled() const { return bEnabled; }

    // Widget 연결
    UClass* GetSpecificWidgetClass() const override;

    // Serialization
    void Serialize(bool bInIsLoading, JSON& InOutHandle) override;

protected:
    // Fog Parameters
    float FogDensity = 0.02f;
    float FogHeightFalloff = 0.2f;
    float StartDistance = 0.0f;
    float FogCutoffDistance = 10000.0f;
    float FogMaxOpacity = 1.0f;
    bool bEnabled = true;

    // 기본 하늘색 안개
    FVector4 FogInscatteringColor = FVector4(0.447f, 0.639f, 1.0f, 1.0f);

    void DuplicateSubObjects(UObject* DuplicatedObject) override;
};
