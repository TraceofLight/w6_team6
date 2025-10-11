#pragma once
#include "Component/Public/PrimitiveComponent.h"

class UMaterial;

UCLASS()
class UDecalComponent : public USceneComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UDecalComponent, USceneComponent)
public:
    UDecalComponent();
    ~UDecalComponent();
    // 머티리얼 기반으로 전환
    void SetMaterial(UMaterial* InMaterial) { DecalMaterial = InMaterial; }
    UMaterial* GetMaterial() const { return DecalMaterial; }

    // 하위 호환: 텍스처 선택
    void SetTexture(class UTexture* InTexture) { DecalTexture = InTexture; }
    class UTexture* GetTexture() const { return DecalTexture; }

    // Primitive에서 쓰던 인터페이스 최소 복원
    bool IsVisible() const { return bVisible; }
    void SetVisibility(bool bVisibility) { bVisible = bVisibility; }
    // DecalPass가 쓰는 바운딩 볼륨
    const IBoundingVolume* GetBoundingBox();

    // 전용 속성 위젯 연결
    UClass* GetSpecificWidgetClass() const override;

protected:
    UMaterial* DecalMaterial = nullptr;
    class UTexture* DecalTexture = nullptr;
    // 내부 보유 (Primitive가 아니므로 직접 가짐)
    IBoundingVolume* BoundingBox = nullptr;
    bool bVisible = true;
};