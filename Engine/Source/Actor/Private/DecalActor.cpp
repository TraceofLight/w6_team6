#include "pch.h"
#include "Actor/Public/DecalActor.h"

IMPLEMENT_CLASS(ADecalActor, AActor)
ADecalActor::ADecalActor()
{
    SetCanTick(true);        // 런타임/PIE 틱
    SetTickInEditor(true);   // 에디터에서도 틱 허용
}

UClass* ADecalActor::GetDefaultRootComponent()
{
    return UDecalComponent::StaticClass();
}