#include "pch.h"
#include "Actor/Public/HeightFogActor.h"

#include "Component/Public/HeightFogComponent.h"

IMPLEMENT_CLASS(AHeightFogActor, AActor)

AHeightFogActor::AHeightFogActor()
{
}

UClass* AHeightFogActor::GetDefaultRootComponent()
{
	return UHeightFogComponent::StaticClass();
}
