#include "pch.h"
#include "Component/Public/FireBallComponent.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UFireBallComponent, UPrimitiveComponent)

void UFireBallComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    USceneComponent::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // Load using FJsonSerializer
        FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity, 0.02f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "Radius", Radius, 0.2f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "RadiusFallOff", RadiusFallOff, 0.0f, false);
        //FJsonSerializer::ReadVector4(InOutHandle, "Color", Color, FVector4{ 1.0f, 0.0f, 1.0f, 1.0f }, false);

        // Color 로드
        JSON ColorArray;
        if (FJsonSerializer::ReadArray(InOutHandle, "Color", ColorArray, nullptr, false))
        {
            if (ColorArray.size() >= 4)
            {
                Color.X = static_cast<float>(ColorArray.at(0).ToFloat());
                Color.Y = static_cast<float>(ColorArray.at(1).ToFloat());
                Color.Z = static_cast<float>(ColorArray.at(2).ToFloat());
                Color.W = static_cast<float>(ColorArray.at(3).ToFloat());
            }
        }
    }
    else
    {
        // Save
        InOutHandle["Intensity"] = Intensity;
        InOutHandle["Radius"] = Radius;
        InOutHandle["RadiusFallOff"] = RadiusFallOff;

        // Color 저장
        JSON ColorArray = JSON::Make(JSON::Class::Array);
        ColorArray.append(Color.X);
        ColorArray.append(Color.Y);
        ColorArray.append(Color.Z);
        ColorArray.append(Color.W);
        InOutHandle["Color"] = ColorArray;
    }
}

UObject* UFireBallComponent::Duplicate()
{
	UFireBallComponent* FireBallComponent = Cast<UFireBallComponent>(Super::Duplicate());

	FireBallComponent->Intensity = Intensity;
	FireBallComponent->Radius = Radius;
	FireBallComponent->RadiusFallOff = RadiusFallOff;
	FireBallComponent->Color = Color;

	return FireBallComponent;
}

void UFireBallComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}