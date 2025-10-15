#include "pch.h"
#include "Actor/Public/FireballActor.h"
#include "Component/Public/PointLightComponent.h"

IMPLEMENT_CLASS(AFireballActor, AActor)

AFireballActor::AFireballActor() = default;

UClass* AFireballActor::GetDefaultRootComponent()
{
	return UPointLightComponent::StaticClass();
}
