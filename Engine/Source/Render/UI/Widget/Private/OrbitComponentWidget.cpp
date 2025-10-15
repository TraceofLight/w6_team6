#include "pch.h"
#include "Render/UI/Widget/Public/OrbitComponentWidget.h"
#include "Component/Public/OrbitComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UOrbitComponentWidget, UWidget)

UOrbitComponent* UOrbitComponentWidget::FindSelectedOrbitComponent() const
{
    AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
    if (!SelectedActor)
    {
        return nullptr;
    }

    // 현재 선택된 컴포넌트가 OrbitComponent이면 우선 사용
    if (auto* SelectedComp = Cast<UOrbitComponent>(GEditor->GetEditorModule()->GetSelectedComponent()))
    {
        return SelectedComp;
    }

    // 아니면 액터에서 첫 번째 OrbitComponent 찾기
    for (UActorComponent* Comp : SelectedActor->GetOwnedComponents())
    {
        if (auto* Orbit = Cast<UOrbitComponent>(Comp))
        {
            return Orbit;
        }
    }

    return nullptr;
}

void UOrbitComponentWidget::RenderWidget()
{
    UOrbitComponent* Orbit = FindSelectedOrbitComponent();
    if (!Orbit)
    {
        return;
    }

    ImGui::Text("Orbit");
    ImGui::Separator();

    // Radius
    float Radius = Orbit->GetRadius();
    if (ImGui::DragFloat("Radius", &Radius, 1.0f, 1.0f, 10000.0f, "%.1f"))
    {
        Orbit->SetRadius(Radius);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Radius of the orbit circle");
    }

    // Segments
    int32 Segments = Orbit->GetSegments();
    if (ImGui::DragInt("Segments", &Segments, 1, 3, 360))
    {
        Orbit->SetSegments(Segments);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Number of line segments to form the orbit circle (higher = smoother)");
    }

    ImGui::Separator();

    // Color
    FVector4 CurrentColor = Orbit->GetColor();
    float ColorArr[4] = {CurrentColor.X, CurrentColor.Y, CurrentColor.Z, CurrentColor.W};
    if (ImGui::ColorEdit4("Orbit Color", ColorArr))
    {
        Orbit->SetOrbitColor(FVector4(ColorArr[0], ColorArr[1], ColorArr[2], ColorArr[3]));
    }

    ImGui::Separator();

    // Presets
    ImGui::Text("Presets:");
    if (ImGui::Button("Small (50)"))
    {
        Orbit->SetRadius(50.0f);
    }
    ImGui::SameLine();

    if (ImGui::Button("Medium (100)"))
    {
        Orbit->SetRadius(100.0f);
    }
    ImGui::SameLine();

    if (ImGui::Button("Large (200)"))
    {
        Orbit->SetRadius(200.0f);
    }

    if (ImGui::Button("Low Quality (16)"))
    {
        Orbit->SetSegments(16);
    }
    ImGui::SameLine();

    if (ImGui::Button("Normal (64)"))
    {
        Orbit->SetSegments(64);
    }
    ImGui::SameLine();

    if (ImGui::Button("High Quality (128)"))
    {
        Orbit->SetSegments(128);
    }
}
