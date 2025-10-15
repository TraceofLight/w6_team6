#pragma once
#include "Component/Public/ActorComponent.h"

/**
 * @brief Projectile Movement Component
 * 발사체의 물리적 움직임을 시뮬레이션하기 위한 컴포넌트 (중력, 속도, 바운스 등)
 * 
 * @param InitialSpeed 초기 속도
 * @param MaxSpeed 최대 속도
 * @param Velocity 현재 속도 벡터
 * @param ProjectileGravityScale 중력 스케일 (1.0 = 정상, 0.0 = 중력 없음)
 * @param bRotationFollowsVelocity 회전이 속도 방향을 따르는지 여부
 * @param bShouldBounce 충돌 시 반발 여부
 * @param Bounciness 반발 계수 (0.0 ~ 1.0)
 * @param Friction 마찰 계수 (0.0 ~ 1.0)
 */
UCLASS()
class UProjectileMovementComponent :
    public UActorComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UProjectileMovementComponent, UActorComponent)

public:
    UProjectileMovementComponent();
    ~UProjectileMovementComponent() override;

    void BeginPlay() override;
    void TickComponent() override;

    UObject* Duplicate() override;

    // Movement Properties Setters
    void SetInitialSpeed(float InSpeed) { InitialSpeed = max(0.0f, InSpeed); }
    void SetMaxSpeed(float InSpeed) { MaxSpeed = max(0.0f, InSpeed); }
    void SetVelocity(const FVector& InVelocity) { Velocity = InVelocity; }
    void SetProjectileGravityScale(float InScale) { ProjectileGravityScale = InScale; }
    void SetRotationFollowsVelocity(bool bInFollow) { bRotationFollowsVelocity = bInFollow; }
    void SetShouldBounce(bool bInBounce) { bShouldBounce = bInBounce; }
    void SetBounciness(float InBounciness) { Bounciness = clamp(InBounciness, 0.0f, 1.0f); }
    void SetFriction(float InFriction) { Friction = clamp(InFriction, 0.0f, 1.0f); }
    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

    // Movement Properties Getters
    float GetInitialSpeed() const { return InitialSpeed; }
    float GetMaxSpeed() const { return MaxSpeed; }
    FVector GetVelocity() const { return Velocity; }
    float GetProjectileGravityScale() const { return ProjectileGravityScale; }
    bool GetRotationFollowsVelocity() const { return bRotationFollowsVelocity; }
    bool GetShouldBounce() const { return bShouldBounce; }
    float GetBounciness() const { return Bounciness; }
    float GetFriction() const { return Friction; }
    bool IsEnabled() const { return bEnabled; }

    // Widget 연결
    UClass* GetSpecificWidgetClass() const override;

    // Serialization
    void Serialize(bool bInIsLoading, JSON& InOutHandle) override;

protected:
    // Movement Parameters
    float InitialSpeed = 50.0f;
    float MaxSpeed = 200.0f;
    FVector Velocity = FVector(0, 0, 0);

    float ProjectileGravityScale = 1.0f; // 1.0 = normal gravity
    bool bRotationFollowsVelocity = true;
    bool bShouldBounce = false;
    float Bounciness = 0.3f; // 0.0 ~ 1.0
    float Friction = 0.0f; // 0.0 ~ 1.0
    bool bEnabled = true;

    bool bHasBegunPlay = false;

    void DuplicateSubObjects(UObject* DuplicatedObject) override;
    void UpdateComponentVelocity(float DeltaTime);
};
