#pragma once
#include "Component/Public/SceneComponent.h"

class UBillBoardComponent;
class UDecalComponent;
class UTexture;

/**
 * @brief 스포트라이트 효과를 위한 통합 컴포넌트
 * - 원점 표시용 빌보드 아이콘 (스포트라이트 심볼)
 * - 바닥으로 투사되는 데칼 (빛 영역 표시)
 * - SpotAngle과 ProjectionDistance 기반 자동 크기 계산
 */
UCLASS()
class USemiLightComponent : public USceneComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(USemiLightComponent, USceneComponent)

public:
    USemiLightComponent();
    ~USemiLightComponent() override;

    void BeginPlay() override;
    void TickComponent() override;

    // Component Setters (Actor에서 생성 후 설정)
    void SetIconComponent(UBillBoardComponent* InIconComponent);
    void SetDecalComponent(UDecalComponent* InDecalComponent);

    // Public API
    void SetDecalTexture(UTexture* InTexture);
    void SetSpotAngle(float InAngle);
    void SetBlendFactor(float InFactor);
    void SetProjectionDistance3D(const FVector& InDistance);
    void SetDecalBoxSize(const FVector& InSize);

    float GetSpotAngle() const { return SpotAngle; }
    FVector GetProjectionDistance3D() const { return ProjectionDistance3D; }
    float GetBlendFactor() const { return BlendFactor; }
    FVector GetDecalBoxSize() const { return DecalBoxSize; }
    float GetMaxAngleForDecalBox() const;

    UBillBoardComponent* GetIconComponent() const { return IconComponent; }
    UDecalComponent* GetDecalComponent() const { return DecalComponent; }
    // Serialization
    void Serialize(bool bInIsLoading, JSON& InOutHandle) override;

private:
    void UpdateDecalProperties();
    void UpdateDecalBoxFromScale();

    // Child Components
    UBillBoardComponent* IconComponent = nullptr;
    UDecalComponent* DecalComponent = nullptr;

    // Properties
    float SpotAngle = 45.0f;
    FVector ProjectionDistance3D = FVector(10.f, 10.f, 10.f);
    float BlendFactor = 1.0f;
    FVector DecalBoxSize = FVector(10.0f, 10.0f, 10.0f);
};
