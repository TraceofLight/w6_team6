#pragma once
#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"

class UMaterial;
class USpriteMaterial;
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
    void SetMaterial(UMaterial* InMaterial);
    UMaterial* GetMaterial() const { return DecalMaterial; }

    void SetSpriteMaterial(USpriteMaterial* InMaterial);
    USpriteMaterial* GetSpriteMaterial() const { return SpriteMaterial; }

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


    // ============ Fade API ============
    void TickComponent() override;

    void SetFadeEnabled(bool bEnabled);
    bool IsFadeEnabled() const { return bFadeEnabled; }
    void SetFadeLoop(bool bLoop);
    bool IsFadeLoop() const { return bFadeLoop; }
    void SetFadeDurations(float InSeconds, float OutSeconds);
    float GetFadeInDuration() const { return FadeInDuration; }
    float GetFadeOutDuration() const { return FadeOutDuration; }
    float GetFadeAlpha() const { return FadeAlpha; }
protected:
    UMaterial* DecalMaterial = nullptr;
    class UTexture* DecalTexture = nullptr;
    USpriteMaterial* SpriteMaterial = nullptr;

    bool bVisible = true;
    // 내부 보유 (Primitive가 아니므로 직접 가짐)
    IBoundingVolume* BoundingBox = nullptr;

    // 내부 half-size (OBB Extents)
    FVector DecalExtent = FVector(GDecalUnitHalfExtent, GDecalUnitHalfExtent, GDecalUnitHalfExtent);
private:
    void RefreshTickState();
    bool NeedsTick() const;

    // ============ Fade ============
    enum class EFadePhase : uint8 
    {
        Idle, 
        FadingIn, 
        FadingOut 
    };

    void StartFadeIn();
    void StartFadeOut();
    void StopFade(bool bToMaxOpacity);
    void UpdateFade(float DeltaTime);

    // Fade settings
    bool  bFadeEnabled = false;
    bool  bFadeLoop = false;
    float FadeInDuration = 0.5f;
    float FadeOutDuration = 0.5f;
    float MinOpacity = 0.0f;
    float MaxOpacity = 1.0f;

    // Runtime
    float     FadeAlpha = 1.0f;     // [0..1], shader로 전달될 값
    EFadePhase FadePhase = EFadePhase::Idle;
    float     PhaseTime = 0.0f;
};
