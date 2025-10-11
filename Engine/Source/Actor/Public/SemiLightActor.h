#pragma once
#include "Actor/Public/Actor.h"
#include "Core/Public/Class.h"

class UBillBoardComponent;
class UDecalComponent;

UCLASS()
class ASemiLightActor : public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(ASemiLightActor, AActor)

public:
    ASemiLightActor();
    ~ASemiLightActor() override;

    void SetDecalTexture(UTexture* InTexture);
    void SetSpotAngle(float InAngle);
    void SetProjectionDistance(float InDistance);

private:
    void UpdateDecalProperties();

    USceneComponent* DefaultSceneRoot = nullptr;
    UBillBoardComponent* IconComponent = nullptr;
    UDecalComponent* DecalComponent = nullptr;

    float SpotAngle = 45.0f;
    float ProjectionDistance = 500.0f;
};