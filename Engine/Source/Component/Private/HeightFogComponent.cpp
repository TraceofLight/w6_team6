#include "pch.h"
#include "Component/Public/HeightFogComponent.h"
#include "Render/UI/Widget/Public/HeightFogComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UHeightFogComponent, USceneComponent)

UHeightFogComponent::UHeightFogComponent()
{
    // 기본값은 헤더에서 설정됨
}

UHeightFogComponent::~UHeightFogComponent()
{
}

void UHeightFogComponent::TickComponent()
{
    USceneComponent::TickComponent();
}

UClass* UHeightFogComponent::GetSpecificWidgetClass() const
{
    return UHeightFogComponentWidget::StaticClass();
}

void UHeightFogComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    USceneComponent::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // Load using FJsonSerializer
        FJsonSerializer::ReadFloat(InOutHandle, "FogDensity", FogDensity, 0.02f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "FogHeightFalloff", FogHeightFalloff, 0.2f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "StartDistance", StartDistance, 0.0f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "FogCutoffDistance", FogCutoffDistance, 10000.0f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "FogMaxOpacity", FogMaxOpacity, 1.0f, false);
        FJsonSerializer::ReadBool(InOutHandle, "bEnabled", bEnabled, true, false);

        // Color 로드
        JSON ColorArray;
        if (FJsonSerializer::ReadArray(InOutHandle, "FogInscatteringColor", ColorArray, nullptr, false))
        {
            if (ColorArray.size() >= 4)
            {
                FogInscatteringColor.X = static_cast<float>(ColorArray.at(0).ToFloat());
                FogInscatteringColor.Y = static_cast<float>(ColorArray.at(1).ToFloat());
                FogInscatteringColor.Z = static_cast<float>(ColorArray.at(2).ToFloat());
                FogInscatteringColor.W = static_cast<float>(ColorArray.at(3).ToFloat());
            }
        }
    }
    else
    {
        // Save
        InOutHandle["FogDensity"] = FogDensity;
        InOutHandle["FogHeightFalloff"] = FogHeightFalloff;
        InOutHandle["StartDistance"] = StartDistance;
        InOutHandle["FogCutoffDistance"] = FogCutoffDistance;
        InOutHandle["FogMaxOpacity"] = FogMaxOpacity;
        InOutHandle["bEnabled"] = bEnabled;

        // Color 저장
        JSON ColorArray = JSON::Make(JSON::Class::Array);
        ColorArray.append(FogInscatteringColor.X);
        ColorArray.append(FogInscatteringColor.Y);
        ColorArray.append(FogInscatteringColor.Z);
        ColorArray.append(FogInscatteringColor.W);
        InOutHandle["FogInscatteringColor"] = ColorArray;
    }
}

UObject* UHeightFogComponent::Duplicate()
{
    UHeightFogComponent* Duplicated = Cast<UHeightFogComponent>(USceneComponent::Duplicate());
    if (Duplicated)
    {
        Duplicated->FogDensity = FogDensity;
        Duplicated->FogHeightFalloff = FogHeightFalloff;
        Duplicated->StartDistance = StartDistance;
        Duplicated->FogCutoffDistance = FogCutoffDistance;
        Duplicated->FogMaxOpacity = FogMaxOpacity;
        Duplicated->FogInscatteringColor = FogInscatteringColor;
        Duplicated->bEnabled = bEnabled;
    }
    return Duplicated;
}

void UHeightFogComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
    USceneComponent::DuplicateSubObjects(DuplicatedObject);
}
