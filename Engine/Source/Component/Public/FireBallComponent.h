#pragma once
#include "Component/Public/PrimitiveComponent.h"
#include "Source/Global/Vector.h"

UCLASS()
class UFireBallComponent : public UPrimitiveComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UFireBallComponent, UPrimitiveComponent)

public:
    UFireBallComponent()
    {

    }

    float GetIntensity() const { return Intensity; }
    float GetRadius() const { return Radius; }
    float GetRadiusFallOff() const { return RadiusFallOff; }
    FVector4 GetLightColor() const { return Color; }

    void SetIntensity(float In) { Intensity = In; }
    void SetRadius(float In) { Radius = In; }
    void SetRadiusFallOff(float In) { RadiusFallOff = In; }
    void SetLightColor(const FVector4& In) { Color = In; }

    void Serialize(const bool bInIsLoading, JSON& InOutHandle);

    UObject* Duplicate();
    void DuplicateSubObjects(UObject* DuplicatedObject);

private:
    float Intensity = 1.0f;
    float Radius = 5.0f;
    float RadiusFallOff = 0.5f;
    FVector4 Color = FVector4{ 1.0f, 1.0f, 1.0f, 1.0f };
};