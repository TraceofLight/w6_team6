#include "pch.h"
#include "Render/UI/Widget/Public/ProjectileMovementComponentWidget.h"
#include "Component/Public/ProjectileMovementComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UProjectileMovementComponentWidget, UWidget)

UProjectileMovementComponent*
UProjectileMovementComponentWidget::FindSelectedProjectileMovementComponent() const
{
    AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
    if (!SelectedActor)
    {
        return nullptr;
    }

    // 현재 선택된 컴포넌트가 ProjectileMovementComponent이면 우선 사용
    if (auto* SelectedComp =
        Cast<UProjectileMovementComponent>(GEditor->GetEditorModule()->GetSelectedComponent()))
    {
        return SelectedComp;
    }

    // 아니면 액터에서 첫 번째 ProjectileMovementComponent 찾기
    for (UActorComponent* Comp : SelectedActor->GetOwnedComponents())
    {
        if (auto* Proj = Cast<UProjectileMovementComponent>(Comp))
        {
            return Proj;
        }
    }

    return nullptr;
}

void UProjectileMovementComponentWidget::RenderWidget()
{
    UProjectileMovementComponent* Proj = FindSelectedProjectileMovementComponent();
    if (!Proj)
    {
        return;
    }

    ImGui::Text("Projectile Movement");
    ImGui::Separator();

    // 활성화 토글
    bool bEnabled = Proj->IsEnabled();
    if (ImGui::Checkbox("Enabled", &bEnabled))
    {
        Proj->SetEnabled(bEnabled);
    }

    ImGui::Separator();

    // Initial Speed
    float InitialSpeed = Proj->GetInitialSpeed();
    if (ImGui::DragFloat("Initial Speed", &InitialSpeed, 10.0f, 0.0f, 10000.0f, "%.1f cm/s"))
    {
        Proj->SetInitialSpeed(InitialSpeed);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Initial velocity magnitude when projectile starts");
    }

    // Max Speed
    float MaxSpeed = Proj->GetMaxSpeed();
    if (ImGui::DragFloat("Max Speed", &MaxSpeed, 10.0f, 0.0f, 20000.0f, "%.1f cm/s"))
    {
        Proj->SetMaxSpeed(MaxSpeed);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Maximum velocity magnitude");
    }

    ImGui::Separator();

    // Gravity Scale
    float GravityScale = Proj->GetProjectileGravityScale();
    if (ImGui::SliderFloat("Gravity Scale", &GravityScale, 0.0f, 3.0f, "%.2f"))
    {
        Proj->SetProjectileGravityScale(GravityScale);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Multiplier for gravity (0 = no gravity, 1 = normal gravity)");
    }

    // Friction
    float Friction = Proj->GetFriction();
    if (ImGui::SliderFloat("Friction", &Friction, 0.0f, 1.0f, "%.3f"))
    {
        Proj->SetFriction(Friction);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Air friction applied to velocity");
    }

    ImGui::Separator();

    // Rotation Follows Velocity
    bool bRotationFollowsVelocity = Proj->GetRotationFollowsVelocity();
    if (ImGui::Checkbox("Rotation Follows Velocity", &bRotationFollowsVelocity))
    {
        Proj->SetRotationFollowsVelocity(bRotationFollowsVelocity);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically orient rotation to movement direction");
    }

    ImGui::Separator();

    // Bounce Settings
    bool bShouldBounce = Proj->GetShouldBounce();
    if (ImGui::Checkbox("Should Bounce", &bShouldBounce))
    {
        Proj->SetShouldBounce(bShouldBounce);
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Whether projectile should bounce on impact");
    }

    if (bShouldBounce)
    {
        float Bounciness = Proj->GetBounciness();
        if (ImGui::SliderFloat("Bounciness", &Bounciness, 0.0f, 1.0f, "%.2f"))
        {
            Proj->SetBounciness(Bounciness);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Coefficient of restitution (0 = no bounce, 1 = perfect bounce)");
        }
    }

    ImGui::Separator();

    // Current Velocity (Read-only)
    FVector Velocity = Proj->GetVelocity();
    ImGui::Text("Current Velocity:");
    ImGui::Text("  X: %.1f  Y: %.1f  Z: %.1f", Velocity.X, Velocity.Y, Velocity.Z);
    ImGui::Text("  Speed: %.1f cm/s", Velocity.Length());
}
