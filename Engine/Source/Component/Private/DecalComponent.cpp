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
    // 기본은 비활성. 토글 시에만 활성화
    SetCanTick(false);
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
    {
        Super::Serialize(bInIsLoading, InOutHandle);
        if (bInIsLoading)
        {
            // 기존 사이즈 로드
            FVector SavedSize = FVector(1.f, 1.f, 1.f);
            FJsonSerializer::ReadVector(InOutHandle, "DecalSize", SavedSize, FVector(1.f, 1.f, 1.f));
            SetDecalSize(SavedSize);

            // Fade 설정 로드(기본값 안전)
            FJsonSerializer::ReadBool(InOutHandle, "FadeEnabled", bFadeEnabled, false);
            FJsonSerializer::ReadBool(InOutHandle, "FadeLoop", bFadeLoop, false);
            FJsonSerializer::ReadFloat(InOutHandle, "FadeInDuration", FadeInDuration, 0.5f);
            FJsonSerializer::ReadFloat(InOutHandle, "FadeOutDuration", FadeOutDuration, 0.5f);
            FJsonSerializer::ReadFloat(InOutHandle, "MinOpacity", MinOpacity, 0.0f);
            FJsonSerializer::ReadFloat(InOutHandle, "MaxOpacity", MaxOpacity, 1.0f);

            // 상태 정렬
            SetFadeEnabled(bFadeEnabled);
        }
        else
        {
            InOutHandle["DecalSize"] = FJsonSerializer::VectorToJson(GetDecalSize());
            InOutHandle["FadeEnabled"] = bFadeEnabled;
            InOutHandle["FadeLoop"] = bFadeLoop;
            InOutHandle["FadeInDuration"] = FadeInDuration;
            InOutHandle["FadeOutDuration"] = FadeOutDuration;
            InOutHandle["MinOpacity"] = MinOpacity;
            InOutHandle["MaxOpacity"] = MaxOpacity;
        }

    }
}

void UDecalComponent::TickComponent()
{
    Super::TickComponent();
    if (!bFadeEnabled) return;
    UpdateFade(DT);
}

void UDecalComponent::SetFadeEnabled(bool bEnabled)
{
    bFadeEnabled = bEnabled;
    if (bFadeEnabled)
    {
        SetCanTick(true);
        StartFadeIn();
        // 데칼 페이드가 켜질 때, 소유 액터의 에디터 틱을 자동으로 보장
        // 액터의 에디터 틱에 다른 로직이 있고, 에디터에서 작동하지 않는걸 바란다면 삭제해야함.
        if (GWorld && GWorld->GetWorldType() == EWorldType::Editor)
        {
            if (AActor* Owner = GetOwner())
            {
                Owner->SetTickInEditor(true);
                Owner->SetCanTick(true);
            }
        }
    }
    else
    {
        StopFade(true);
        SetCanTick(false);
        if (GWorld && GWorld->GetWorldType() == EWorldType::Editor)
        {
            if (AActor* Owner = GetOwner())
            {
                Owner->SetTickInEditor(false);
                Owner->SetCanTick(false);
            }
        }
    }
}
void UDecalComponent::SetFadeLoop(bool bLoop) 
{ 
    const bool bPrev = bFadeLoop;
    bFadeLoop = bLoop;

    if (!bFadeEnabled)
        return;

    if (bFadeLoop)
    {
        // 혹시 꺼져있다면 틱 보장
        SetCanTick(true);

        // FadeOut이 끝나 Idle + MinOpacity 상태였다면 즉시 FadeIn 재시작
        const float Eps = 1e-4f;
        if (FadePhase == EFadePhase::Idle && FadeAlpha <= (MinOpacity + Eps))
        {
            StartFadeIn();
        }
        // FadingOut 중이라면 별도 처리 불필요: UpdateFade에서 완료 시 StartFadeIn으로 넘어감
    }
}

void UDecalComponent::SetFadeDurations(float InSeconds, float OutSeconds)
{
    FadeInDuration = std::max(0.f, InSeconds);
    FadeOutDuration = std::max(0.f, OutSeconds);
}

void UDecalComponent::StartFadeIn() 
{ 
    FadePhase = EFadePhase::FadingIn;
    PhaseTime = 0.f; 
}

void UDecalComponent::StartFadeOut() 
{ 
    FadePhase = EFadePhase::FadingOut;
    PhaseTime = 0.f; 
}

void UDecalComponent::StopFade(bool bToMax)
{
    FadePhase = EFadePhase::Idle;
    PhaseTime = 0.f;
    FadeAlpha = bToMax ? MaxOpacity : MinOpacity;
}

void UDecalComponent::UpdateFade(float DeltaTime)
{
    auto Lerp01 = [](float a, float b, float t) { return a + (b - a) * std::clamp(t, 0.f, 1.f); };

    if (FadePhase == EFadePhase::FadingIn)
    {
        const float Dur = std::max(0.0001f, FadeInDuration);
        PhaseTime += DeltaTime;
        const float t = std::clamp(PhaseTime / Dur, 0.f, 1.f);
        FadeAlpha = Lerp01(MinOpacity, MaxOpacity, t);
        if (t >= 1.f)
        {
            bFadeLoop ? StartFadeOut() : StartFadeOut();  // 단발도 Out까진 수행
        }
    }
    else if (FadePhase == EFadePhase::FadingOut)
    {
        const float Dur = std::max(0.0001f, FadeOutDuration);
        PhaseTime += DeltaTime;
        const float t = std::clamp(PhaseTime / Dur, 0.f, 1.f);
        FadeAlpha = Lerp01(MaxOpacity, MinOpacity, t);
        if (t >= 1.f)
        {
            bFadeLoop ? StartFadeIn() : StopFade(false);
        }
    }

}




