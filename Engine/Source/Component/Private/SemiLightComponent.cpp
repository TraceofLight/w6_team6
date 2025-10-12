#include "pch.h"
#include "Component/Public/SemiLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Actor/Public/Actor.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(USemiLightComponent, USceneComponent)

USemiLightComponent::USemiLightComponent()
{
	// Scale 변경 감지를 위해 Tick 활성화
	bCanEverTick = true;
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
}

void USemiLightComponent::TickComponent()
{
	Super::TickComponent();

	SetWorldScale3D({1.0f, 1.0f, 1.0f});
	UpdateDecalBoxFromScale();
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

void USemiLightComponent::SetProjectionDistance3D(const FVector& InDistance)
{
	ProjectionDistance3D = InDistance;
	UpdateDecalProperties();

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

	// 광원 위치(원점)부터 박스가 시작되도록 중심을 X 방향으로 BoxDepth/2만큼 이동
	const FVector DecalRelativeLocation = FVector(BoxDepth * 0.5f, 0.0f, 0.0f);

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

void USemiLightComponent::UpdateDecalBoxFromScale()
{
	// Scale에 따라 DecalBoxSize 업데이트
	FVector CurrentScale = GetWorldScale3D();

	// DecalComponent는 Y축 90도 회전 → 로컬 X축=부모 -Z, 로컬 Y=부모 Y, 로컬 Z=부모 X
	// DecalBoxSize는 DecalComponent 로컬 좌표계 기준
	DecalBoxSize.X = ProjectionDistance3D.X * CurrentScale.Z;  // DecalBox 깊이 (투사 방향) = 부모 Z Scale
	DecalBoxSize.Y = ProjectionDistance3D.Y * CurrentScale.Y;  // DecalBox Y 반경 = 부모 Y Scale
	DecalBoxSize.Z = ProjectionDistance3D.Z * CurrentScale.X;  // DecalBox Z 반경 = 부모 X Scale

	// Decal 프로퍼티 업데이트
	UpdateDecalProperties();

	// SpotAngle도 최대치를 넘지 않도록 재조정
	SetSpotAngle(SpotAngle);
}

void USemiLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// 1) 자식 포인터 복구 (부착정보가 아직 없어도 Owner의 컴포넌트 목록에서 찾아 연결)
		IconComponent = nullptr;
		DecalComponent = nullptr;
		if (AActor* Owner = GetOwner())
		{
			for (UActorComponent* Comp : Owner->GetOwnedComponents())
			{
				if (auto* BB = Cast<UBillBoardComponent>(Comp)) { IconComponent = BB; }
				else if (auto* DC = Cast<UDecalComponent>(Comp)) { DecalComponent = DC; }
			}
		}

		// 2) 세미라이트 고유 파라미터 복원(없으면 현재값 유지)
		FJsonSerializer::ReadFloat(InOutHandle, "SpotAngle", SpotAngle, SpotAngle, false);
		FJsonSerializer::ReadFloat(InOutHandle, "ProjectionDistanceX", ProjectionDistance3D.X, ProjectionDistance3D.X, false);
		FJsonSerializer::ReadFloat(InOutHandle, "ProjectionDistanceY", ProjectionDistance3D.Y, ProjectionDistance3D.Y, false);
		FJsonSerializer::ReadFloat(InOutHandle, "ProjectionDistanceZ", ProjectionDistance3D.Z, ProjectionDistance3D.Z, false);
		FJsonSerializer::ReadFloat(InOutHandle, "BlendFactor", BlendFactor, BlendFactor, false);
		FVector SavedBox = DecalBoxSize;
		FJsonSerializer::ReadVector(InOutHandle, "DecalBoxSize", SavedBox, DecalBoxSize, false);
		DecalBoxSize = SavedBox;

		// 3) 데칼에 상태 반영 + 투사용 회전/오프셋 재계산
		if (DecalComponent)
		{
			DecalComponent->SetSpotAngle(SpotAngle);
			DecalComponent->SetBlendFactor(BlendFactor);

			// 투사 방향 정합(과거 저장본 보호용)
			if (DecalComponent->GetRelativeRotation() == FVector::ZeroVector())
				DecalComponent->SetRelativeRotation(FVector(0.f, 90.f, 0.f));

			// 박스 크기/오프셋 동기화
			DecalComponent->SetDecalSize(DecalBoxSize);
			UpdateDecalProperties();

			// 가시화(안전)
			DecalComponent->SetVisibility(true);
		}
	}
	else
	{
		InOutHandle["SpotAngle"] = SpotAngle;
		InOutHandle["ProjectionDistanceX"] = ProjectionDistance3D.X;
		InOutHandle["ProjectionDistanceY"] = ProjectionDistance3D.Y;
		InOutHandle["ProjectionDistanceZ"] = ProjectionDistance3D.Z;
		InOutHandle["BlendFactor"] = BlendFactor;
		InOutHandle["DecalBoxSize"] = FJsonSerializer::VectorToJson(DecalBoxSize);
	}
}


UObject* USemiLightComponent::Duplicate()
{
	auto* Copy = Cast<USemiLightComponent>(Super::Duplicate());
	if (AActor* Owner = Copy->GetOwner())
	{
		Copy->IconComponent = nullptr;
		Copy->DecalComponent = nullptr;
		for (UActorComponent* Comp : Owner->GetOwnedComponents())
		{
			if (auto* BB = Cast<UBillBoardComponent>(Comp)) { Copy->IconComponent = BB; }
			else if (auto* DC = Cast<UDecalComponent>(Comp)) { Copy->DecalComponent = DC; }
		}
		if (Copy->DecalComponent)
		{
			Copy->DecalComponent->SetSpotAngle(Copy->SpotAngle);
			Copy->DecalComponent->SetBlendFactor(Copy->BlendFactor);
			Copy->DecalComponent->SetDecalSize(Copy->DecalBoxSize);
			Copy->UpdateDecalProperties();
			Copy->DecalComponent->SetVisibility(true);
		}
	}
	return Copy;
}

void USemiLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}