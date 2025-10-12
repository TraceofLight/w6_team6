#include "pch.h"
#include "Actor/Public/SemiLightActor.h"
#include "Component/Public/SemiLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(ASemiLightActor, AActor)

ASemiLightActor::ASemiLightActor()
{
	bCanEverTick = false;  // USemiLightComponent가 Tick을 처리
	
	// SemiLightComponent 생성 및 루트로 설정
	//SemiLightComponent = CreateDefaultSubobject<USemiLightComponent>(FName("SemiLightComponent"));
	//SetRootComponent(SemiLightComponent);


}

ASemiLightActor::~ASemiLightActor() = default;

void ASemiLightActor::SetDecalTexture(UTexture* InTexture)
{
	if (SemiLightComponent)
	{
		SemiLightComponent->SetDecalTexture(InTexture);
	}
}

void ASemiLightActor::SetSpotAngle(float InAngle)
{
	if (SemiLightComponent)
	{
		SemiLightComponent->SetSpotAngle(InAngle);
	}
}

void ASemiLightActor::SetProjectionDistance(float InDistance)
{
	if (SemiLightComponent)
	{
		SemiLightComponent->SetProjectionDistance(InDistance);
	}
}
UClass* ASemiLightActor::GetDefaultRootComponent()
{
	return USemiLightComponent::StaticClass();
}
void ASemiLightActor::InitializeComponents()
{
	Super::InitializeComponents();

	// 자식 컴포넌트 생성
	UBillBoardComponent* IconComponent = CreateDefaultSubobject<UBillBoardComponent>(FName("IconComponent"));
	UTexture* Icon = UAssetManager::GetInstance().CreateTexture("Asset/Icon/PointLight_64x.png");
	IconComponent->SetSprite(Icon);

	UDecalComponent* DecalComponent = CreateDefaultSubobject<UDecalComponent>(FName("DecalComponent"));
	UTexture* Light = UAssetManager::GetInstance().CreateTexture("Asset/Texture/semilight.png");
	DecalComponent->SetTexture(Light);

	// SemiLightComponent에 전달
	if (SemiLightComponent)
	{
		SemiLightComponent->SetIconComponent(IconComponent);
		SemiLightComponent->SetDecalComponent(DecalComponent);
		SemiLightComponent->SetWorldLocation(GetActorLocation());
	}
	SetRootComponent(SemiLightComponent);
	// 부모-자식 관계 설정
	if (IconComponent)
	{
		IconComponent->SetParentAttachment(GetRootComponent());
	}

	if (DecalComponent)
	{
		DecalComponent->SetParentAttachment(GetRootComponent());
		// 데칼 초기 방향
		DecalComponent->SetRelativeRotation(FVector(0.0f, 90.0f, 0.0f));
	}
}