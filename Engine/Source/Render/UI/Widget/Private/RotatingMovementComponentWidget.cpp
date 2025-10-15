#include "pch.h"
#include "Render/UI/Widget/Public/RotatingMovementComponentWidget.h"
#include "Component/Public/RotatingMovementComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(URotatingMovementComponentWidget, UWidget)

URotatingMovementComponent*
URotatingMovementComponentWidget::FindSelectedRotatingMovementComponent() const
{
    AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
    if (!SelectedActor)
    {
        return nullptr;
    }

    // 현재 선택된 컴포넌트가 RotatingMovementComponent이면 우선 사용
    if (auto* SelectedComp =
        Cast<URotatingMovementComponent>(GEditor->GetEditorModule()->GetSelectedComponent()))
    {
        return SelectedComp;
    }

    // 아니면 액터에서 첫 번째 RotatingMovementComponent 찾기
    for (UActorComponent* Comp : SelectedActor->GetOwnedComponents())
    {
        if (auto* Rotation = Cast<URotatingMovementComponent>(Comp))
        {
            return Rotation;
        }
    }

    return nullptr;
}

void URotatingMovementComponentWidget::RenderWidget()
{
    URotatingMovementComponent* Rotation = FindSelectedRotatingMovementComponent();
    if (!Rotation)
    {
        return;
    }

    ImGui::Text("Rotating Movement");
    ImGui::Separator();

    // 활성화 토글
    bool bEnabled = Rotation->IsEnabled();
    if (ImGui::Checkbox("Enabled", &bEnabled))
    {
        Rotation->SetEnabled(bEnabled);
    }

    ImGui::Separator();

    // Rotation Rate
    FVector RotationRate = Rotation->GetRotationRate();
    bool bChanged = false;

    ImGui::Text("Rotation Rate (Degrees/Second):");
    float X = RotationRate.X;
    if (ImGui::DragFloat("Pitch (X)", &X, 1.0f, -360.0f, 360.0f, "%.1f"))
    {
        RotationRate.X = X;
        bChanged = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Rotation around X axis (Pitch)");
    }

    float Y = RotationRate.Y;
    if (ImGui::DragFloat("Yaw (Y)", &Y, 1.0f, -360.0f, 360.0f, "%.1f"))
    {
        RotationRate.Y = Y;
        bChanged = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Rotation around Y axis (Yaw)");
    }

    float Z = RotationRate.Z;
    if (ImGui::DragFloat("Roll (Z)", &Z, 1.0f, -360.0f, 360.0f, "%.1f"))
    {
        RotationRate.Z = Z;
        bChanged = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Rotation around Z axis (Roll)");
    }

    if (bChanged)
    {
        Rotation->SetRotationRate(RotationRate);
    }

    ImGui::Separator();

    // Pivot Translation
    FVector PivotTranslation = Rotation->GetPivotTranslation();
    float PivotArr[3] = {PivotTranslation.X, PivotTranslation.Y, PivotTranslation.Z};
    if (ImGui::DragFloat3("Pivot Trans", PivotArr, 1.0f, -1000.0f, 1000.0f, "%.1f"))
    {
        Rotation->SetPivotTranslation(FVector(PivotArr[0], PivotArr[1], PivotArr[2]));
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Local offset for rotation pivot point");
    }

    // Rotate in Local Space
    bool bRotateInLocalSpace = Rotation->GetRotateInLocalSpace();
    if (ImGui::Checkbox("Rotate in Local Space", &bRotateInLocalSpace))
    {
        Rotation->SetRotateInLocalSpace(bRotateInLocalSpace);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Rotate in local space (true) or world space (false)");
    }

    ImGui::Separator();

    // Presets
    ImGui::Text("Presets:");
    if (ImGui::Button("Slow Spin"))
    {
        Rotation->SetRotationRate(FVector(0.0f, 30.0f, 0.0f));
    }
    ImGui::SameLine();

    if (ImGui::Button("Medium Spin"))
    {
        Rotation->SetRotationRate(FVector(0.0f, 90.0f, 0.0f));
    }
    ImGui::SameLine();

    if (ImGui::Button("Fast Spin"))
    {
        Rotation->SetRotationRate(FVector(0.0f, 180.0f, 0.0f));
    }

    if (ImGui::Button("Tumble"))
    {
        Rotation->SetRotationRate(FVector(45.0f, 45.0f, 45.0f));
    }
    ImGui::SameLine();

    if (ImGui::Button("Stop"))
    {
        Rotation->SetRotationRate(FVector(0.0f, 0.0f, 0.0f));
    }
}
