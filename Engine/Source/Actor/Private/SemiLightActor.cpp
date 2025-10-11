#include "pch.h"
#include "Actor/Public/SemiLightActor.h"

#include "Component/Public/BillBoardComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Component/Public/SceneComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"

IMPLEMENT_CLASS(ASemiLightActor, AActor)

ASemiLightActor::ASemiLightActor()
{
    // 1. 루트 컴포넌트 생성
    DefaultSceneRoot = AddComponent<USceneComponent>(FName("DefaultSceneRoot"));
    SetRootComponent(DefaultSceneRoot);

    // 2. 자식 컴포넌트 생성 및 부착
    IconComponent = AddComponent<UBillBoardComponent>(FName("IconComponent"));
    IconComponent->SetParentAttachment(GetRootComponent());

    DecalComponent = AddComponent<UDecalComponent>(FName("DecalComponent"));
    DecalComponent->SetParentAttachment(GetRootComponent());

    // 3. 에셋 로드 및 설정
    UAssetManager& AssetManager = UAssetManager::GetInstance();
    auto IconSrv = AssetManager.GetTexture("Icon/SpotLight_64x.png");
    if (IconSrv)
    {
        IconComponent->SetSprite({ FName("Icon/SpotLight_64x.png"), IconSrv.Get() });
    }

    UTexture* DecalTexture = AssetManager.CreateTexture("Texture/texture.png", FName("DefaultDecal"));
    if (DecalTexture)
    {
        DecalComponent->SetTexture(DecalTexture);
    }

    // 4. 데칼 초기 방향 설정 (하방으로 -90도 회전)
    DecalComponent->SetRelativeRotation(FVector(-90.0f, 0.0f, 0.0f));

    // 5. 초기 프로퍼티 적용
    UpdateDecalProperties();
}

ASemiLightActor::~ASemiLightActor() = default;

void ASemiLightActor::SetDecalTexture(UTexture* InTexture)
{
    if (DecalComponent)
    {
        DecalComponent->SetTexture(InTexture);
    }
}

void ASemiLightActor::SetSpotAngle(float InAngle)
{
    SpotAngle = InAngle;
    UpdateDecalProperties();
}

void ASemiLightActor::SetProjectionDistance(float InDistance)
{
    ProjectionDistance = InDistance;
    UpdateDecalProperties();
}

void ASemiLightActor::UpdateDecalProperties()
{
    if (!DecalComponent)
    {
        return;
    }

    // 크기 계산
    const float Radius = tanf(FVector::GetDegreeToRadian(SpotAngle * 0.5f)) * ProjectionDistance;
    const float BoxDepth = ProjectionDistance;

    // 위치 계산 (데칼 박스는 중심 기준이므로, 투사 방향으로 깊이의 절반만큼 이동)
    const FVector DecalRelativeLocation = FVector(0.0f, 0.0f, BoxDepth * 0.5f);

    // 프로퍼티 설정
    DecalComponent->SetRelativeLocation(DecalRelativeLocation);
    DecalComponent->SetDecalSize(FVector(BoxDepth, Radius * 2.0f, Radius * 2.0f));
}
