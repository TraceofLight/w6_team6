#pragma once
#include "Widget.h"

class UTargetActorTransformWidget
	: public UWidget
{
public:
	void Initialize() override;
	void Update() override;
	void RenderWidget() override;
	void PostProcess() override;

	void UpdateTransformFromActor();
	void ApplyTransformToActor() const;

	// Special Member Function
	UTargetActorTransformWidget();
	~UTargetActorTransformWidget() override;

private:
	AActor* SelectedActor;

	FVector EditLocation;
	FVector EditRotation;
	FVector EditScale;
	bool bScaleChanged = false;
	bool bRotationChanged = false;
	bool bPositionChanged = false;
	bool bNeedsBVHUpdate = false;  // 드래그 종료 시 BVH 업데이트 플래그
	uint64 LevelMemoryByte;
	uint32 LevelObjectCount;
};
