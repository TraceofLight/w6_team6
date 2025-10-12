#include "pch.h"
#include "Component/Public/DecalComponent.h"
#include "Physics/Public/OBB.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Texture/Public/Texture.h"
#include "Texture/Public/SpriteMaterial.h"
#include "Render/UI/Widget/Public/DecalComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"
#include "Level/Public/Level.h"
#include "Core/Public/ObjectIterator.h"

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
    // TODO - 추후에 지우고, 머터리얼로만 동작하도록
    SetTexture(UAssetManager::GetInstance().CreateTexture(FName("Asset/Texture/texture.png"), FName("Texture")));
    // 기본은 비활성. 토글 시에만 활성화
    SetCanTick(false);
}

UDecalComponent::~UDecalComponent()
{
    SafeDelete(BoundingBox);
    // TODO - 에셋 매니저에서 해제해주는 것인지?
    // 복제시 텍스쳐는 얕은 복사이므로, 소멸자에서 지우면 안됨!
    // SafeDelete(DecalTexture);
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

void UDecalComponent::MarkAsDirty()
{
    // 부모 클래스의 MarkAsDirty 호출 (트랜스폼 더티 플래그 설정)
    Super::MarkAsDirty();

    // SceneBVH는 Editor/ActorDetailWidget에서 드래그 종료 시 RefitComponent로 업데이트
    // 여기서는 Transform Dirty Flag만 설정하고, BVH 업데이트는 드래그 완료 시에만 수행
}

void UDecalComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // 1) 사이즈/페이드 기존 로직
        FVector SavedSize = FVector(1.f, 1.f, 1.f);
        FJsonSerializer::ReadVector(InOutHandle, "DecalSize", SavedSize, FVector(1.f, 1.f, 1.f));
        SetDecalSize(SavedSize);

        FJsonSerializer::ReadBool(InOutHandle, "FadeEnabled", bFadeEnabled, false);
        FJsonSerializer::ReadBool(InOutHandle, "FadeLoop", bFadeLoop, false);
        FJsonSerializer::ReadFloat(InOutHandle, "FadeInDuration", FadeInDuration, 0.5f);
        FJsonSerializer::ReadFloat(InOutHandle, "FadeOutDuration", FadeOutDuration, 0.5f);
        FJsonSerializer::ReadFloat(InOutHandle, "MinOpacity", MinOpacity, 0.0f);
        FJsonSerializer::ReadFloat(InOutHandle, "MaxOpacity", MaxOpacity, 1.0f);
        SetFadeEnabled(bFadeEnabled);

        // 2) 머티리얼/텍스처 복원
        FString MatPath, TexPath;
        bool bHasMat = FJsonSerializer::ReadString(InOutHandle, "MaterialPath", MatPath, "", false);
        bool bHasTex = FJsonSerializer::ReadString(InOutHandle, "TexturePath", TexPath, "", false);

        if (bHasMat && !MatPath.empty())
        {
            // a) 기존 머티리얼 풀에서 찾기(StaticMeshComponent 방식과 동일)
            UMaterial* Found = nullptr;
            for (TObjectIterator<UMaterial> It; It; ++It)
            {
                UMaterial* Mat = *It; if (!Mat) continue;
                if (UTexture* Diff = Mat->GetDiffuseTexture())
                {
                    if (Diff->GetFilePath() == FName(MatPath))
                    {
                        Found = Mat;
                        break;
                    }
                }
            }
            if (Found)
            {
                SetMaterial(Found);
            }
            else
            {
                // b) 못 찾으면 최소 머티리얼 생성(디퓨즈만 세팅)
                UTexture* Diff = UAssetManager::GetInstance().CreateTexture(FName(MatPath));
                if (Diff)
                {
                    UMaterial* Mat = NewObject<UMaterial>();
                    Mat->SetDiffuseTexture(Diff);
                    SetMaterial(Mat);
                }
            }
        }
        else if (bHasTex && !TexPath.empty())
        {
            // 텍스처만 저장된 경우 폴백
            UTexture* Tex = UAssetManager::GetInstance().CreateTexture(FName(TexPath));
            if (Tex) { SetTexture(Tex); }
        }

        // 3) 추가 속성(선택)
        float Spot = SpotAngle, Blend = BlendFactor;
        FJsonSerializer::ReadFloat(InOutHandle, "SpotAngle", Spot, SpotAngle, false);
        FJsonSerializer::ReadFloat(InOutHandle, "BlendFactor", Blend, BlendFactor, false);
        SpotAngle = Spot;
        BlendFactor = std::clamp(Blend, 0.f, 1.f);
    }
    else
    {
        // 1) 기존 저장 로직 유지
        InOutHandle["DecalSize"] = FJsonSerializer::VectorToJson(GetDecalSize());
        InOutHandle["FadeEnabled"] = bFadeEnabled;
        InOutHandle["FadeLoop"] = bFadeLoop;
        InOutHandle["FadeInDuration"] = FadeInDuration;
        InOutHandle["FadeOutDuration"] = FadeOutDuration;
        InOutHandle["MinOpacity"] = MinOpacity;
        InOutHandle["MaxOpacity"] = MaxOpacity;

        // 2) 머티리얼/텍스처 경로 저장(우선순위: 머티리얼 → 텍스처)
        if (DecalMaterial && DecalMaterial->GetDiffuseTexture())
        {
            InOutHandle["MaterialPath"] = DecalMaterial->GetDiffuseTexture()->GetFilePath().ToString();
        }
        else if (DecalTexture)
        {
            InOutHandle["TexturePath"] = DecalTexture->GetFilePath().ToString();
        }

        // 3) 추가 속성(선택)
        InOutHandle["SpotAngle"] = SpotAngle;
        InOutHandle["BlendFactor"] = BlendFactor;
    }
}

void UDecalComponent::TickComponent()
{
    Super::TickComponent();

    if (bFadeEnabled)
    {
        UpdateFade(DT);
    }

    if (SpriteMaterial)
    {
        SpriteMaterial->Update(DT);
    }
}

void UDecalComponent::SetFadeEnabled(bool bEnabled)
{
    bFadeEnabled = bEnabled;
    if (bFadeEnabled)
    {
        StartFadeIn();
    }
    else
    {
        StopFade(true);
    }

    RefreshTickState();
}
void UDecalComponent::SetFadeLoop(bool bLoop) 
{ 
    const bool bPrev = bFadeLoop;
    bFadeLoop = bLoop;

    if (!bFadeEnabled)
        return;

    if (bFadeLoop)
    {
        // FadeOut이 끝나 Idle + MinOpacity 상태였다면 즉시 FadeIn 재시작
        const float Eps = 1e-4f;
        if (FadePhase == EFadePhase::Idle && FadeAlpha <= (MinOpacity + Eps))
        {
            StartFadeIn();
        }
        // FadingOut 중이라면 별도 처리 불필요: UpdateFade에서 완료 시 StartFadeIn으로 넘어감
    }

    RefreshTickState();
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

void UDecalComponent::SetMaterial(UMaterial* InMaterial)
{
    DecalMaterial = InMaterial;
    SpriteMaterial = Cast<USpriteMaterial>(InMaterial);

    if (SpriteMaterial && SpriteMaterial->IsAutoPlay() && !SpriteMaterial->IsPlaying())
    {
        SpriteMaterial->Play();
    }

    RefreshTickState();
}

void UDecalComponent::SetSpriteMaterial(USpriteMaterial* InMaterial)
{
    SetMaterial(InMaterial);
}

void UDecalComponent::RefreshTickState()
{
    const bool bShouldTick = NeedsTick();
    SetCanTick(bShouldTick);

    if (GWorld && GWorld->GetWorldType() == EWorldType::Editor)
    {
        if (AActor* Owner = GetOwner())
        {
            Owner->SetTickInEditor(bShouldTick);
            Owner->SetCanTick(bShouldTick);
        }
    }
}

bool UDecalComponent::NeedsTick() const
{
    if (bFadeEnabled)
    {
        return true;
    }

    if (SpriteMaterial && SpriteMaterial->RequiresTick())
    {
        return true;
    }

    return false;
}

UObject* UDecalComponent::Duplicate()
{
    UDecalComponent* DecalComponent = Cast<UDecalComponent>(Super::Duplicate());
    
    // 데칼 고유 상태 복사
    DecalComponent->DecalMaterial = DecalMaterial;         // 공유 (자원 매니저 관리)
    DecalComponent->DecalTexture = DecalTexture;          // 현재 코드가 소유 삭제(SafeDelete)라면 별도 정책 고려(아래 주의 참조)
    DecalComponent->bVisible = bVisible;
    DecalComponent->DecalExtent = DecalExtent;
    
    // 페이드 설정/상태 복사(PIE에서 동일 미리보기 원하면 권장)
    DecalComponent->bFadeEnabled = bFadeEnabled;
    DecalComponent->bFadeLoop = bFadeLoop;
    DecalComponent->FadeInDuration = FadeInDuration;
    DecalComponent->FadeOutDuration = FadeOutDuration;
    DecalComponent->MinOpacity = MinOpacity;
    DecalComponent->MaxOpacity = MaxOpacity;
    DecalComponent->FadeAlpha = FadeAlpha;
    DecalComponent->FadePhase = FadePhase;
    DecalComponent->PhaseTime = PhaseTime;

    // 바운딩 박스 복제(반드시 새로 생성)
    {
        FOBB* Old = static_cast<FOBB*>(BoundingBox);
        const FVector Center = Old ? Old->Center : FVector(0.f, 0.f, 0.f);
        const FMatrix Rot = Old ? Old->ScaleRotation : FMatrix::Identity();
        const FVector Extents = DecalExtent; // 또는 Old ? Old->Extents : DecalExtent

        DecalComponent->BoundingBox = new FOBB(Center, Extents, Rot);

        // 현재 상태와 동기화(Extents/월드 변환 재적용)
        FOBB* NewOBB = static_cast<FOBB*>(DecalComponent->BoundingBox);
        NewOBB->Extents = DecalComponent->DecalExtent;
        NewOBB->Update(DecalComponent->GetWorldTransformMatrix());
    }

    // Enable 상태면 Tick 보장
    if (DecalComponent->bFadeEnabled)
    {
        DecalComponent->SetCanTick(true);
    }
    return DecalComponent;
}