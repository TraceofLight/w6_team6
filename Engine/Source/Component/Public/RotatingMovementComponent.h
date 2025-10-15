#pragma once
#include "Component/Public/ActorComponent.h"

/**
 * @brief Rotating Movement Component
 *
 * 언리얼 엔진의 URotatingMovementComponent를 참조한 구현
 * 컴포넌트를 지정된 회전 속도로 지속적으로 회전시킵니다.
 *
 * @note SceneComponent를 상속받지 않으며, Owner Actor의 회전을 직접 수정합니다.
 *
 * @param RotationRate 초당 회전 속도 (Degrees/Second)
 * @param PivotTranslation 회전 중심점 오프셋 (로컬 좌표)
 * @param bRotateInLocalSpace 로컬 공간에서 회전할지 월드 공간에서 회전할지 여부
 */
UCLASS()
class URotatingMovementComponent : public UActorComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(URotatingMovementComponent, UActorComponent)

public:
	URotatingMovementComponent();
	~URotatingMovementComponent() override;

	void BeginPlay() override;
	void TickComponent() override;

	UObject* Duplicate() override;

	// Rotation Properties Setters
	void SetRotationRate(const FVector& InRate) { RotationRate = InRate; }
	void SetPivotTranslation(const FVector& InTranslation) { PivotTranslation = InTranslation; }
	void SetRotateInLocalSpace(bool bInLocal) { bRotateInLocalSpace = bInLocal; }
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	// Rotation Properties Getters
	FVector GetRotationRate() const { return RotationRate; }
	FVector GetPivotTranslation() const { return PivotTranslation; }
	bool GetRotateInLocalSpace() const { return bRotateInLocalSpace; }
	bool IsEnabled() const { return bEnabled; }

	// Updated Component
	void SetUpdatedComponent(USceneComponent* InComponent);
	USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }

	// Widget 연결
	UClass* GetSpecificWidgetClass() const override;

	// Serialization
	void Serialize(bool bInIsLoading, JSON& InOutHandle) override;

protected:
	// Rotation Parameters
	FVector RotationRate = FVector(0.0f, 90.0f, 0.0f); // Degrees/Second (Pitch, Yaw, Roll)
	FVector PivotTranslation = FVector(0, 0, 0);       // Local space offset
	bool bRotateInLocalSpace = true;
	bool bEnabled = true;

	bool bHasBegunPlay = false;

	// The component we move and update. If this is nullptr, we'll use the RootComponent of our Owner.
	USceneComponent* UpdatedComponent = nullptr;

	// Temporary storage for component identification during duplication
	FString PendingUpdatedComponentClassName;
	int32 PendingUpdatedComponentIndex = -1;

	void DuplicateSubObjects(UObject* DuplicatedObject) override;
	void ResolvePendingUpdatedComponent();
};
