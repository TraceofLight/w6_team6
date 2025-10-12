#pragma once
#include "Actor/Public/Actor.h"
#include "Core/Public/Class.h"

class USemiLightComponent;

/**
 * @brief 스포트라이트 효과를 표현하는 액터
 * - USemiLightComponent를 루트 컴포넌트로 소유
 * - 모든 기능은 USemiLightComponent에 위임
 */
UCLASS()
class ASemiLightActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(ASemiLightActor, AActor)

public:
	ASemiLightActor();
	~ASemiLightActor() override;

	// Getter
	USemiLightComponent* GetSemiLightComponent() const { return SemiLightComponent; }

	// 하위 호환성을 위한 Forwarding API
	void SetDecalTexture(UTexture* InTexture);
	void SetSpotAngle(float InAngle);
	void SetProjectionDistance(float InDistance);
	virtual UClass* GetDefaultRootComponent() override;
	void InitializeComponents() override;
private:
	USemiLightComponent* SemiLightComponent = nullptr;
};
