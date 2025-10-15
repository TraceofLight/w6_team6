#include "pch.h"
#include "Render/UI/Widget/Public/UPointLightComponentWidget.h"
#include "Component/Public/PointLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UPointLightComponentWidget, UWidget)

UPointLightComponent*
UPointLightComponentWidget::FindSelectedFireballComponent() const
{
    AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
    if (!SelectedActor)
    {
        return nullptr;
    }

    // 현재 선택된 컴포넌트가 FireballComponent이면 우선 사용
    if (auto* SelectedComp =
        Cast<UPointLightComponent>(GEditor->GetEditorModule()->GetSelectedComponent()))
    {
        return SelectedComp;
    }

    // 아니면 액터에서 첫 번째 FireballComponent 찾기
    for (UActorComponent* Comp : SelectedActor->GetOwnedComponents())
    {
        if (auto* Fireball = Cast<UPointLightComponent>(Comp))
        {
            return Fireball;
        }
    }

    return nullptr;
}

void UPointLightComponentWidget::RenderWidget()
{
    UPointLightComponent* Fireball = FindSelectedFireballComponent();
    if (!Fireball)
    {
        return;
    }

    ImGui::Text("Point Light (Fireball)");
    ImGui::Separator();

    // Intensity
    float Intensity = Fireball->GetIntensity();
    if (ImGui::DragFloat("Intensity", &Intensity, 1.0f, 0.0f, 500.0f, "%.1f"))
    {
        Fireball->SetIntensity(Intensity);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Light intensity / brightness");
    }

    // Radius
    float Radius = Fireball->GetRadius();
    if (ImGui::DragFloat("Radius", &Radius, 10.0f, 0.0f, 5000.0f, "%.1f"))
    {
        Fireball->SetRadius(Radius);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Light influence radius");
    }

    // Radius Falloff
    float RadiusFalloff = Fireball->GetRadiusFalloff();
    if (ImGui::SliderFloat("Radius Falloff", &RadiusFalloff, 0.5f, 5.0f, "%.2f"))
    {
        Fireball->SetRadiusFalloff(RadiusFalloff);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Light attenuation falloff exponent (higher = sharper falloff)");
    }

    ImGui::Separator();

    // Light Color
    FVector LightColor = Fireball->GetLightColor();
    float ColorArray[3] = {LightColor.X, LightColor.Y, LightColor.Z};
    if (ImGui::ColorEdit3("Light Color", ColorArray))
    {
        Fireball->SetLightColor(FVector(ColorArray[0], ColorArray[1], ColorArray[2]));
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("RGB color of the light");
    }

    ImGui::Separator();

    // Current Location (Read-only)
    FVector Location = Fireball->GetWorldLocation();
    ImGui::Text("World Location:");
    ImGui::Text("  X: %.1f  Y: %.1f  Z: %.1f", Location.X, Location.Y, Location.Z);
}
