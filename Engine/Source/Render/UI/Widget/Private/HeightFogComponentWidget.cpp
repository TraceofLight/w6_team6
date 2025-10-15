#include "pch.h"
#include "Render/UI/Widget/Public/HeightFogComponentWidget.h"
#include "Component/Public/HeightFogComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UHeightFogComponentWidget, UWidget)

UHeightFogComponent* UHeightFogComponentWidget::FindSelectedHeightFogComponent() const
{
    AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
    if (!SelectedActor) return nullptr;

    // 현재 선택된 컴포넌트가 HeightFogComponent이면 우선 사용
    if (auto* SelectedComp = Cast<UHeightFogComponent>(GEditor->GetEditorModule()->GetSelectedComponent()))
        return SelectedComp;

    // 아니면 액터에서 첫 번째 HeightFogComponent 찾기
    for (UActorComponent* Comp : SelectedActor->GetOwnedComponents())
        if (auto* Fog = Cast<UHeightFogComponent>(Comp))
            return Fog;

    return nullptr;
}

void UHeightFogComponentWidget::RenderWidget()
{
    UHeightFogComponent* Fog = FindSelectedHeightFogComponent();
    if (!Fog) return;

    ImGui::Text("Height Fog");
    ImGui::Separator();

    // 활성화 토글
    bool bEnabled = Fog->IsEnabled();
    if (ImGui::Checkbox("Enabled", &bEnabled))
    {
        Fog->SetEnabled(bEnabled);
    }

    ImGui::Separator();

    // Fog Density
    float Density = Fog->GetFogDensity();
    if (ImGui::SliderFloat("Fog Density", &Density, 0.0f, 1.0f, "%.3f"))
    {
        Fog->SetFogDensity(Density);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Overall fog density (higher = thicker fog)");

    // Fog Height Falloff
    float Falloff = Fog->GetFogHeightFalloff();
    if (ImGui::SliderFloat("Height Falloff", &Falloff, 0.001f, 2.0f, "%.3f"))
    {
        Fog->SetFogHeightFalloff(Falloff);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("How quickly fog density decreases with height");

    // Start Distance
    float StartDist = Fog->GetStartDistance();
    if (ImGui::DragFloat("Start Distance", &StartDist, 10.0f, 0.0f, 10000.0f, "%.1f"))
    {
        Fog->SetStartDistance(StartDist);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Distance from camera where fog begins");

    // Cutoff Distance
    float CutoffDist = Fog->GetFogCutoffDistance();
    if (ImGui::DragFloat("Cutoff Distance", &CutoffDist, 100.0f, 0.0f, 50000.0f, "%.1f"))
    {
        Fog->SetFogCutoffDistance(CutoffDist);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Maximum distance where fog reaches full opacity");

    // Max Opacity
    float MaxOpacity = Fog->GetFogMaxOpacity();
    if (ImGui::SliderFloat("Max Opacity", &MaxOpacity, 0.0f, 1.0f, "%.2f"))
    {
        Fog->SetFogMaxOpacity(MaxOpacity);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Maximum fog opacity (0 = transparent, 1 = opaque)");

    ImGui::Separator();

    // Fog Inscattering Color
    FVector4 Color = Fog->GetFogInscatteringColor();
    float ColorArr[4] = { Color.X, Color.Y, Color.Z, Color.W };
    if (ImGui::ColorEdit4("Fog Color", ColorArr, ImGuiColorEditFlags_Float))
    {
        Fog->SetFogInscatteringColor(FVector4(ColorArr[0], ColorArr[1], ColorArr[2], ColorArr[3]));
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Fog inscattering color (RGB + Alpha)");

    ImGui::Separator();

    // 프리셋 버튼
    ImGui::Text("Presets:");

    if (ImGui::Button("Light Morning"))
    {
        Fog->SetFogDensity(0.01f);
        Fog->SetFogHeightFalloff(0.1f);
        Fog->SetFogInscatteringColor(FVector4(1.0f, 0.9f, 0.7f, 1.0f)); // 밝은 주황
    }
    ImGui::SameLine();

    if (ImGui::Button("Heavy Mist"))
    {
        Fog->SetFogDensity(0.1f);
        Fog->SetFogHeightFalloff(0.5f);
        Fog->SetFogInscatteringColor(FVector4(0.8f, 0.8f, 0.9f, 1.0f)); // 회백색
    }
    ImGui::SameLine();

    if (ImGui::Button("Deep Blue"))
    {
        Fog->SetFogDensity(0.05f);
        Fog->SetFogHeightFalloff(0.2f);
        Fog->SetFogInscatteringColor(FVector4(0.2f, 0.4f, 0.8f, 1.0f)); // 깊은 파랑
    }

    if (ImGui::Button("Sunset"))
    {
        Fog->SetFogDensity(0.03f);
        Fog->SetFogHeightFalloff(0.15f);
        Fog->SetFogInscatteringColor(FVector4(1.0f, 0.5f, 0.3f, 1.0f)); // 오렌지
    }
    ImGui::SameLine();

    if (ImGui::Button("Toxic Green"))
    {
        Fog->SetFogDensity(0.08f);
        Fog->SetFogHeightFalloff(0.3f);
        Fog->SetFogInscatteringColor(FVector4(0.3f, 1.0f, 0.3f, 1.0f)); // 녹색
    }
    ImGui::SameLine();

    if (ImGui::Button("Volcano Ash"))
    {
        Fog->SetFogDensity(0.12f);
        Fog->SetFogHeightFalloff(0.4f);
        Fog->SetFogInscatteringColor(FVector4(0.5f, 0.3f, 0.2f, 1.0f)); // 회갈색
    }
}
