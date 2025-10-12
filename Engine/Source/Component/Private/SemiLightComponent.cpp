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
		// BlendFactor 초기화
		DecalComponent->SetBlendFactor(BlendFactor);
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

	// Decal 크기를 DecalBoxSize에 반영
	const FVector NewDecalSize = DecalComponent->GetDecalSize();
	DecalBoxSize = NewDecalSize;

	// 새로운 박스 크기에 맞는 최대 각도 계산
	const float MaxAngle = GetMaxAngleForDecalBox();

	// 현재 각도가 최대치를 초과하면 자동으로 조정
	if (SpotAngle > MaxAngle)
	{
		SpotAngle = MaxAngle;

		// DecalComponent의 SpotAngle도 업데이트
		if (DecalComponent)
		{
			DecalComponent->SetSpotAngle(SpotAngle);
		}
	}

	// 4. 프로퍼티가 변경되었으므로, 위치 등을 다시 동기화
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
	// 데칼 박스를 벗어나지 않도록 최대 각도 제한
	const float MaxAngle = GetMaxAngleForDecalBox();
	SpotAngle = std::clamp(InAngle, 0.0f, MaxAngle);

	if (DecalComponent)
	{
		DecalComponent->SetSpotAngle(SpotAngle);
	}
	UpdateDecalProperties();
}

void USemiLightComponent::SetBlendFactor(float InFactor)
{
	BlendFactor = std::clamp(InFactor, 0.f, 1.f);

	// DecalComponent에도 적용
	if (DecalComponent)
	{
		DecalComponent->SetBlendFactor(BlendFactor);
	}
}

void USemiLightComponent::SetProjectionDistance(float InDistance)
{
	ProjectionDistance = InDistance;
	UpdateDecalProperties();
}

void USemiLightComponent::SetDecalBoxSize(const FVector& InSize)
{
	DecalBoxSize = InSize;
	UpdateDecalProperties();
}

void USemiLightComponent::UpdateDecalProperties()
{
	if (!DecalComponent)
	{
		return;
	}

	// 데칼의 로컬 X 방향이 투사 방향(월드 -Z)이므로 DecalBoxSize.X를 사용
	const float BoxDepth = DecalBoxSize.X;

	// 광원 위치(원점)부터 박스가 시작되도록 중심을 -Z 방향으로 BoxDepth/2만큼 이동
	// 박스의 로컬 X=-BoxDepth/2가 월드 Z=0(광원)에 위치
	const FVector DecalRelativeLocation = FVector(0.0f, 0.0f, -BoxDepth * 0.5f);

	// 데칼 박스 크기는 DecalBoxSize 그대로 사용
	DecalComponent->SetRelativeLocation(DecalRelativeLocation);
	DecalComponent->SetDecalSize(DecalBoxSize);
}

float USemiLightComponent::GetMaxAngleForDecalBox() const
{
	// DecalBoxSize를 기준으로 최대 각도 계산
	// 투사 방향: 로컬 X (= DecalBoxSize.X)
	// 평면 방향: 로컬 YZ (= DecalBoxSize.Y, DecalBoxSize.Z)
	const float BoxDepth = DecalBoxSize.X;
	const float BoxRadius = DecalBoxSize.Y * 0.5f;  // Y/Z는 같다고 가정

	if (BoxDepth < 0.001f)
	{
		return 90.0f; // 안전 처리
	}

	// 원점(0,0,0)에서 박스 끝(depth)까지 투사할 때
	// 최대 반지름 = BoxRadius, 깊이 = BoxDepth
	// 최대 각도 = 2 * atan(radius / depth)
	const float MaxAngle = FVector::GetRadianToDegree(2.0f * atanf(BoxRadius / BoxDepth));

	return MaxAngle;
}

void USemiLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: SpotAngle, ProjectionDistance 직렬화 추가 (필요 시)
}
