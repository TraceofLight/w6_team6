#pragma once
#include "Component/Public/PrimitiveComponent.h"

/**
 * @brief Fireball 렌더링 및 Light Volume 효과를 위한 컴포넌트
 * Sphere Mesh로 시각적 표현 (Emissive Glow)
 * Light Pass에서 Point Light 효과 생성
 * Normal은 상수로 처리
 */
UCLASS()
class UPointLightComponent :
    public USceneComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UPointLightComponent, USceneComponent)

public:
    UPointLightComponent();
    ~UPointLightComponent() override;

    void BeginPlay() override;
    void TickComponent() override;

    // Light Properties Getters/Setters
    void SetIntensity(float InIntensity) { Intensity = InIntensity; }
    float GetIntensity() const { return Intensity; }

    void SetRadius(float InRadius) { Radius = InRadius; }
    float GetRadius() const { return Radius; }

    void SetRadiusFalloff(float InFalloff) { RadiusFalloff = InFalloff; }
    float GetRadiusFalloff() const { return RadiusFalloff; }

    void SetLightColor(const FVector& InColor) { LightColor = InColor; }
    FVector GetLightColor() const { return LightColor; }

    void SetEmissiveColor(const FVector& InColor) { EmissiveColor = InColor; }
    FVector GetEmissiveColor() const { return EmissiveColor; }

    // Widget 연결
    UClass* GetSpecificWidgetClass() const override;

    // Serialization
    void Serialize(bool bInIsLoading, JSON& InOutHandle) override;

    // Override for Duplicate
    UObject* Duplicate() override;

private:
    // Light Properties
    float Intensity = 20.0f; // 조명 강도 (권장: 10~100)
    float Radius = 10.0f; // 영향 반경 (월드 단위) - 기본값 1/10로 조정
    float RadiusFalloff = 2.0f; // 감쇠 지수 (1.0 = 선형, 2.0 = 제곱)
    FVector LightColor = FVector(1.0f, 0.5f, 0.1f); // RGB 조명 색상 (주황빛)

    // Visual Properties
    FVector EmissiveColor = FVector(1.0f, 0.3f, 0.0f); // 발광 색상 (시각적 표현)
    float SphereRadius = 20.0f; // Mesh 크기

    bool bHasBegunPlay = false;
};
