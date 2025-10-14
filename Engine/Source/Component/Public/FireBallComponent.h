#pragma once
#include "Component/Public/PrimitiveComponent.h"
#include "Source/Global/Vector.h"

UCLASS()
class UFireBallComponent : public UPrimitiveComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UFireBallComponent, UPrimitiveComponent)

    float Intensity;
    float Radius;
    float RadiusFallOff;
    //FLinearColor
    FVector4 Color;
};