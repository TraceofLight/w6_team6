#pragma once
#include "Component/Public/SceneComponent.h"

class UBillBoardComponent;
class UDecalComponent;
class UTexture;

namespace json { class JSON; }
using JSON = json::JSON;

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
	void SetProjectionDistance(float InDistance);

	// 에디터에서 Decal 위젯을 조작했을 때 호출할 동기화 함수
	void SynchronizePropertiesFromDecal();

	float GetSpotAngle() const { return SpotAngle; }
	float GetProjectionDistance() const { return ProjectionDistance; }

	UBillBoardComponent* GetIconComponent() const { return IconComponent; }
	UDecalComponent* GetDecalComponent() const { return DecalComponent; }

	// Serialization
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	void UpdateDecalProperties();

	// Child Components
	UBillBoardComponent* IconComponent = nullptr;
	UDecalComponent* DecalComponent = nullptr;

	// Properties
	float SpotAngle = 45.0f;
	float ProjectionDistance = 500.0f;
};
