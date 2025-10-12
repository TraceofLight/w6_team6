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

	// 데칼 텍스처 및 SpotAngle 설정
	if (DecalComponent)
	{
		UTexture* DecalTexture = AssetManager.CreateTexture("Texture/texture.png", FName("DefaultDecal"));
		if (DecalTexture)
		{
			DecalComponent->SetTexture(DecalTexture);
		}
		// SemiLight는 원뿔 프러스텀 사용
		DecalComponent->SetSpotAngle(SpotAngle);
	}

	// 초기 프로퍼티 적용
	UpdateDecalProperties();
}

void USemiLightComponent::TickComponent()
{
	Super::TickComponent();
}

void USemiLightComponent::SynchronizePropertiesFromDecal()
{
	if (!DecalComponent)
	{
		return;
	}

	// Decal 크기로부터 프로퍼티 역산
	const FVector DecalSize = DecalComponent->GetDecalSize();
	const float Radius = DecalSize.X * 0.5f; // X, Y는 같다고 가정
	const float BoxDepth = DecalSize.Z;

	// 값 설정
	ProjectionDistance = BoxDepth;
	SpotAngle = FVector::GetRadianToDegree(atanf(Radius / ProjectionDistance)) * 2.0f;

	// 프로퍼티가 변경되었으므로, 위치 등을 다시 동기화
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
	if (DecalComponent)
	{
		DecalComponent->SetSpotAngle(SpotAngle);
	}
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

	// 고정 데칼 크기 설정: 10x10x10
	const float FixedSize = 10.0f;

	// 투사 방향은 -Z이므로, 박스 중심을 -Z 방향으로 FixedSize/2만큼 이동
	const FVector DecalRelativeLocation = FVector(0.0f, 0.0f, -FixedSize * 0.5f);

	// 데칼 박스 크기: 10x10x10으로 고정
	DecalComponent->SetRelativeLocation(DecalRelativeLocation);
	DecalComponent->SetDecalSize(FVector(FixedSize, FixedSize, FixedSize));
}

void USemiLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: SpotAngle, ProjectionDistance 직렬화 추가 (필요 시)
}
