#include "pch.h"
#include "Component/Public/PointLightComponent.h"

#include "Level/Public/Level.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Utility/Public/JsonSerializer.h"
#include "Level/Public/World.h"
#include "Render/UI/Widget/Public/UPointLightComponentWidget.h"

IMPLEMENT_CLASS(UPointLightComponent, USceneComponent)

UPointLightComponent::UPointLightComponent()
{
	// 정적 라이트라면 Tick 불필요
	bCanEverTick = false;
}

UPointLightComponent::~UPointLightComponent()
{
	// Level에서 등록 해제
	if (bHasBegunPlay && GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->UnregisterPointLight(this);
	}
}

void UPointLightComponent::BeginPlay()
{
	Super::BeginPlay();
	bHasBegunPlay = true;

	UE_LOG("FireballComponent: BeginPlay: Owner: %s, Intensity: %.1f, Radius: %.1f",
		GetOwner() ? GetOwner()->GetName().ToString().data() : "nullptr", Intensity, Radius);

	// Level에 Fireball 등록
	if (GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->RegisterPointLight(this);
	}
}

void UPointLightComponent::TickComponent()
{
	Super::TickComponent();
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
	return UPointLightComponentWidget::StaticClass();
}

void UPointLightComponent::Serialize(bool bInIsLoading, JSON& InOutHandle)
{
	USceneComponent::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		UE_LOG("FireballComponent: Serialize: LOADING");

		FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity, 50.0f, false);
		FJsonSerializer::ReadFloat(InOutHandle, "Radius", Radius, 20.0f, false);
		FJsonSerializer::ReadFloat(InOutHandle, "RadiusFalloff", RadiusFalloff, 2.0f, false);
		FJsonSerializer::ReadVector(InOutHandle, "LightColor", LightColor, FVector(1.0f, 0.5f, 0.1f), false);
		FJsonSerializer::ReadVector(InOutHandle, "EmissiveColor", EmissiveColor, FVector(1.0f, 0.3f, 0.0f), false);
		FJsonSerializer::ReadFloat(InOutHandle, "SphereRadius", SphereRadius, 20.0f, false);

		// Reset runtime flag
		bHasBegunPlay = false;

		UE_LOG("FireballComponent: Serialize: Loaded: Intensity=%.1f, Radius=%.1f, Color=(%.2f,%.2f,%.2f)",
			Intensity, Radius, LightColor.X, LightColor.Y, LightColor.Z);
	}
	else
	{
		UE_LOG("FireballComponent: Serialize: SAVING: Intensity=%.1f, Radius=%.1f",
			Intensity, Radius);

		InOutHandle["Intensity"] = Intensity;
		InOutHandle["Radius"] = Radius;
		InOutHandle["RadiusFalloff"] = RadiusFalloff;
		InOutHandle["LightColor"] = FJsonSerializer::VectorToJson(LightColor);
		InOutHandle["EmissiveColor"] = FJsonSerializer::VectorToJson(EmissiveColor);
		InOutHandle["SphereRadius"] = SphereRadius;
	}
}

UObject* UPointLightComponent::Duplicate()
{
	UPointLightComponent* FireballComponent = Cast<UPointLightComponent>(Super::Duplicate());

	// Copy light properties
	FireballComponent->Intensity = Intensity;
	FireballComponent->Radius = Radius;
	FireballComponent->RadiusFalloff = RadiusFalloff;
	FireballComponent->LightColor = LightColor;
	FireballComponent->EmissiveColor = EmissiveColor;
	FireballComponent->SphereRadius = SphereRadius;

	// Reset runtime flag for PIE
	FireballComponent->bHasBegunPlay = false;

	return FireballComponent;
}
