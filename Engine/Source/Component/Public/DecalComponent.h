#pragma once
#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"

class UMaterial;
class ULevel;

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
    void SetVisibility(bool bInVisible)
    {
        if (bVisible != bInVisible)
        {
            bVisible = bInVisible;

            // Level에 가시성 변경 알림
            if (ULevel* Level = GWorld->GetLevel())
            {
                Level->OnDecalVisibilityChanged(this, bInVisible);
            }
        }
    }

    // 데칼 사이즈 API (full size)
    void SetDecalSize(const FVector& InSize)
    {
        // 최소값 클램프 (0에 가까운 값 방지)
        FVector Clamped = { std::max(0.001f, InSize.X), std::max(0.001f, InSize.Y), std::max(0.001f, InSize.Z) };
        DecalExtent = { Clamped.X * 0.5f, Clamped.Y * 0.5f, Clamped.Z * 0.5f };
        MarkAsDirty();
    }
    FVector GetDecalSize() const { return { DecalExtent.X * 2.f, DecalExtent.Y * 2.f, DecalExtent.Z * 2.f }; }
    // DecalPass가 쓰는 바운딩 볼륨
    const IBoundingVolume* GetBoundingBox();

    // 전용 속성 위젯 연결
    UClass* GetSpecificWidgetClass() const override;

    // MarkAsDirty override - Level에 변경 알림
    void MarkAsDirty() override;

    // TODO - 테스트 못해봄
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
protected:
    UMaterial* DecalMaterial = nullptr;
    class UTexture* DecalTexture = nullptr;

    bool bVisible = true;
    // 내부 보유 (Primitive가 아니므로 직접 가짐)
    IBoundingVolume* BoundingBox = nullptr;

    // 내부 half-size (OBB Extents)
    FVector DecalExtent = FVector(GDecalUnitHalfExtent, GDecalUnitHalfExtent, GDecalUnitHalfExtent);
};