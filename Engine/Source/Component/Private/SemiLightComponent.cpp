#include "pch.h"
#include "Component/Public/SemiLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(USemiLightComponent, USceneComponent)

USemiLightComponent::USemiLightComponent()
{
	// 컴포넌트는 Actor의 생성자에서 생성됨
}

USemiLightComponent::~USemiLightComponent() = default;

void USemiLightComponent::SetIconComponent(UBillBoardComponent* InIconComponent)
{
	IconComponent = InIconComponent;
}

void USemiLightComponent::SetDecalComponent(UDecalComponent* InDecalComponent)
{
	DecalComponent = InDecalComponent;
}

void USemiLightComponent::BeginPlay()
{
	Super::BeginPlay();

	// 에셋 로드 및 초기화
	UAssetManager& AssetManager = UAssetManager::GetInstance();

	// 아이콘 스프라이트 설정
	if (IconComponent)
	{
		auto IconSrv = AssetManager.GetTexture("Icon/SpotLight_64x.png");
		if (IconSrv)
		{
			IconComponent->SetSprite({ FName("Icon/SpotLight_64x.png"), IconSrv.Get() });
		}
	}

	// 데칼 텍스처 설정
	if (DecalComponent)
	{
		UTexture* DecalTexture = AssetManager.CreateTexture("Texture/texture.png", FName("DefaultDecal"));
		if (DecalTexture)
		{
			DecalComponent->SetTexture(DecalTexture);
		}
	}

	// 초기 프로퍼티 적용
	UpdateDecalProperties();
}

void USemiLightComponent::TickComponent()
{
	Super::TickComponent();
	UpdateDecalProperties();
}

void USemiLightComponent::SetDecalTexture(UTexture* InTexture)
{
	if (DecalComponent)
	{
		DecalComponent->SetTexture(InTexture);
	}
}

void USemiLightComponent::SetSpotAngle(float InAngle)
{
	SpotAngle = InAngle;
	UpdateDecalProperties();
}

void USemiLightComponent::SetProjectionDistance(float InDistance)
{
	ProjectionDistance = InDistance;
	UpdateDecalProperties();
}

void USemiLightComponent::UpdateDecalProperties()
{
	if (!DecalComponent)
	{
		return;
	}

	// 크기 계산: SpotAngle과 ProjectionDistance 기반
	const float Radius = tanf(FVector::GetDegreeToRadian(SpotAngle * 0.5f)) * ProjectionDistance;
	const float BoxDepth = ProjectionDistance;

	// 위치 계산: 박스 시작면이 컴포넌트 원점에 오도록
	// 투사 방향은 -Z이므로, -Z 방향으로 BoxDepth의 절반만큼 이동
	const FVector DecalRelativeLocation = FVector(0.0f, 0.0f, -BoxDepth * 0.5f);

	// 프로퍼티 적용
	DecalComponent->SetRelativeLocation(DecalRelativeLocation);
	DecalComponent->SetDecalSize(FVector(Radius * 2.0f, Radius * 2.0f, BoxDepth));
}

void USemiLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: SpotAngle, ProjectionDistance 직렬화 추가 (필요 시)
}
