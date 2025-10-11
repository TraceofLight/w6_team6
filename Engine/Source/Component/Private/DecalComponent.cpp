#include "pch.h"
#include "Component/Public/DecalComponent.h"
#include "Physics/Public/OBB.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Render/UI/Widget/Public/DecalComponentWidget.h"

IMPLEMENT_CLASS(UDecalComponent, USceneComponent)

UDecalComponent::UDecalComponent()
{
    // TODO
    // 박스의 초기값은 건들지 말것!
    // 셰이더에서의 데칼의 크기와 직접적으로 연결되어 있는 것이 아닌
    // 초기값이 둘이 같기 때문에 이후의 행렬변환이 있어도 수치가 같은 것!
    // 추후 월드 스케일은 트랜스폼 용도로 남겨두고, 데칼 크기를 별도로 관리하고 싶다면
    // 명시적인 데칼 크기와 범위를 도입하는 것이 좋다.
    BoundingBox = new FOBB(FVector(0.f, 0.f, 0.f), FVector(0.5f, 0.5f, 0.5f), FMatrix::Identity());

    // 머터리얼이 지정되지 않았다면 기본 텍스처로 적용
    SetTexture(UAssetManager::GetInstance().CreateTexture(FName("Asset/Texture/texture.png"), FName("Texture")));
}

UDecalComponent::~UDecalComponent()
{
    SafeDelete(BoundingBox);
    SafeDelete(DecalTexture);
}

const IBoundingVolume* UDecalComponent::GetBoundingBox()
{
    if (BoundingBox)
    {
        BoundingBox->Update(GetWorldTransformMatrix());
    }
    return BoundingBox;
}

UClass* UDecalComponent::GetSpecificWidgetClass() const
{
    return UDecalComponentWidget::StaticClass();
}