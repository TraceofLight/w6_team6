#include "pch.h"
#include "Render/UI/Widget/Public/DecalComponentWidget.h"
#include "Component/Public/DecalComponent.h"
#include "Component/Public/SemiLightComponent.h"
#include "Texture/Public/Material.h"
#include "Texture/Public/Texture.h"
#include "Core/Public/ObjectIterator.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UDecalComponentWidget, UWidget)

UDecalComponent* UDecalComponentWidget::FindSelectedDecalComponent() const
{
    AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
    if (!SelectedActor) return nullptr;

    // 현재 “선택된 컴포넌트”가 데칼이면 우선 사용
    if (auto* SelectedComp = Cast<UDecalComponent>(GEditor->GetEditorModule()->GetSelectedComponent()))
        return SelectedComp;

    // 아니면 액터에서 첫 번째 데칼 찾기
    for (UActorComponent* Comp : SelectedActor->GetOwnedComponents())
        if (auto* Decal = Cast<UDecalComponent>(Comp)) return Decal;

    return nullptr;

}

FString UDecalComponentWidget::GetMaterialDisplayName(UMaterial* Material) const
{
    if (!Material) return "None";

    // 1순위: UObject 이름 (기본 패턴은 필터링)
    FString ObjectName = Material->GetName().ToString();
    if (!ObjectName.empty() && ObjectName.find("Object_") != 0)
    {
        return ObjectName;
    }

    // 2순위: Diffuse 텍스처 파일명
    if (auto* Diffuse = Material->GetDiffuseTexture())
    {
        FString Path = Diffuse->GetFilePath().ToString();
        if (!Path.empty())
        {
            size_t s = Path.find_last_of("/\\");
            size_t d = Path.find_last_of(".");
            if (s != std::string::npos)
            {
                FString FileName = Path.substr(s + 1);
                if (d != std::string::npos && d > s) FileName = FileName.substr(0, d - s - 1);
                return FileName + " (Mat)";
            }
        }
    }

    // 3순위: 다른 텍스처들
    TArray<UTexture*> Textures = {
        Material->GetAmbientTexture(),
        Material->GetSpecularTexture(),
        Material->GetNormalTexture(),
        Material->GetAlphaTexture(),
        Material->GetBumpTexture()
    };
    for (UTexture* Tex : Textures)
    {
        if (!Tex) continue;
        FString Path = Tex->GetFilePath().ToString();
        if (!Path.empty())
        {
            size_t s = Path.find_last_of("/\\");
            size_t d = Path.find_last_of(".");
            if (s != std::string::npos)
            {
                FString FileName = Path.substr(s + 1);
                if (d != std::string::npos && d > s) FileName = FileName.substr(0, d - s - 1);
                return FileName + " (Mat)";
            }
        }
    }

    // 4순위: UUID 기반 포매팅
    return "Material_" + std::to_string(Material->GetUUID());

}

void UDecalComponentWidget::RenderWidget()
{
    UDecalComponent* Decal = FindSelectedDecalComponent();
    if (!Decal) return;

    ImGui::Text("Decal");

    UMaterial* CurrentMat = Decal->GetMaterial();
    FString Preview = CurrentMat ? GetMaterialDisplayName(CurrentMat) : "None";

    if (ImGui::BeginCombo("Material", Preview.c_str()))
    {
        for (TObjectIterator<UMaterial> It; It; ++It)
        {
            UMaterial* Mat = *It; if (!Mat) continue;
            FString Name = GetMaterialDisplayName(Mat);
            bool bSelected = (CurrentMat == Mat);

            if (ImGui::Selectable(Name.c_str(), bSelected))
                Decal->SetMaterial(Mat);

            if (bSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    // 사이즈 편집
    FVector Size = Decal->GetDecalSize();
    float SizeArr[3] = { Size.X, Size.Y, Size.Z };
    if (ImGui::DragFloat3("Decal Size", SizeArr, 0.01f, 0.001f, 1000.0f))
    {
        // 최소값 방어
        SizeArr[0] = std::max(0.001f, SizeArr[0]);
        SizeArr[1] = std::max(0.001f, SizeArr[1]);
        SizeArr[2] = std::max(0.001f, SizeArr[2]);
        Decal->SetDecalSize(FVector(SizeArr[0], SizeArr[1], SizeArr[2]));

        // 부모가 SemiLightComponent이면 역동기화
        if(USemiLightComponent* SemiLight = Cast<USemiLightComponent>(Decal->GetParentAttachment()))
        {
            SemiLight->SynchronizePropertiesFromDecal();
        }
    }
}