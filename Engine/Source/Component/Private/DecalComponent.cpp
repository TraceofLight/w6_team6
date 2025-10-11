#include "pch.h"
#include "Component/Public/DecalComponent.h"
#include "Physics/Public/OBB.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Render/UI/Widget/Public/DecalComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UDecalComponent, USceneComponent)

UDecalComponent::UDecalComponent()
{
    /*
        박스의 초기값은 건들지 말것!
        셰이더에서의 데칼의 크기와 직접적으로 연결되어 있는 것이 아닌
        초기값이 둘이 같기 때문에 이후의 행렬변환이 있어도 수치가 같은 것!
        GDecalUnitHalfExtent를 셰이더로 넘겨서 사용해서
        연결성을 높여도 되지만 우선은 학습을 위해 여기만 사용
    */
    BoundingBox = new FOBB(FVector(0.f, 0.f, 0.f), FVector(GDecalUnitHalfExtent, GDecalUnitHalfExtent, GDecalUnitHalfExtent), FMatrix::Identity());

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
    if (FOBB* OBB = static_cast<FOBB*>(BoundingBox))
    {
        // 데칼 사이즈(Extents)를 OBB에 반영
        OBB->Extents = DecalExtent;
        OBB->Update(GetWorldTransformMatrix());
    }
    return BoundingBox;
}

UClass* UDecalComponent::GetSpecificWidgetClass() const
{
    return UDecalComponentWidget::StaticClass();
}

void UDecalComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    if (bInIsLoading)
    {
        FVector SavedSize = FVector(1.f, 1.f, 1.f);
        FJsonSerializer::ReadVector(InOutHandle, "DecalSize", SavedSize, FVector(1.f, 1.f, 1.f));
        SetDecalSize(SavedSize);
    }
    else
    {
        InOutHandle["DecalSize"] = FJsonSerializer::VectorToJson(GetDecalSize());
    }
}