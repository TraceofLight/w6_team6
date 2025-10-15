#include "pch.h"
#include "Component/Public/ProjectileMovementComponent.h"
#include "Actor/Public/Actor.h"
#include "Component/Public/SceneComponent.h"
#include "Global/Quaternion.h"
#include "Render/UI/Widget/Public/ProjectileMovementComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UProjectileMovementComponent, UActorComponent)

UProjectileMovementComponent::UProjectileMovementComponent()
{
    ComponentType = EComponentType::Actor;
    bCanEverTick = true;
}

UProjectileMovementComponent::~UProjectileMovementComponent() = default;

void UProjectileMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    bHasBegunPlay = true;

    UE_LOG("Projectile: BeginPlay: Owner: %s, InitialSpeed: %.1f",
           GetOwner() ? GetOwner()->GetName().ToString().data() : "nullptr", InitialSpeed);

    // Initialize velocity with InitialSpeed in forward direction
    AActor* OwnerActor = GetOwner();
    if (OwnerActor)
    {
        USceneComponent* RootComponent = OwnerActor->GetRootComponent();
        if (RootComponent)
        {
            FVector Rotation = RootComponent->GetWorldRotation();
            FQuaternion Quat = FQuaternion::FromEuler(Rotation);
            FVector Forward = Quat.RotateVector(FVector::ForwardVector());
            Velocity = Forward * InitialSpeed;
            UE_LOG(
                "ProjectileMovementComponent::BeginPlay - Velocity initialized: (%.1f, %.1f, %.1f)",
                Velocity.X, Velocity.Y, Velocity.Z);
        }
        else
        {
            UE_LOG_WARNING("Projectile: BeginPlay: RootComponent is null");
        }
    }
    else
    {
        UE_LOG_WARNING("Projectile: BeginPlay: Owner is null");
    }
}

void UProjectileMovementComponent::TickComponent()
{
    if (!bEnabled)
    {
        return;
    }

    if (!bHasBegunPlay)
    {
        UE_LOG_WARNING("Projectile: TickComponent: BeginPlay not called yet");
        return;
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    if (DT <= 0.0f)
    {
        return;
    }

    static int TickCount = 0;
    if (TickCount++ < 5)
    {
        UE_LOG("Projectile: TickComponent: DeltaTime: %.3f, Velocity: (%.1f, %.1f, %.1f)",
               DT, Velocity.X, Velocity.Y, Velocity.Z);
    }

    UpdateComponentVelocity(DT);

    // Get root component to update position
    USceneComponent* RootComponent = OwnerActor->GetRootComponent();
    if (!RootComponent)
    {
        return;
    }

    // Update position
    FVector CurrentLocation = RootComponent->GetWorldLocation();
    FVector NewLocation = CurrentLocation + Velocity * DT;
    RootComponent->SetWorldLocation(NewLocation);

    // Update rotation to follow velocity
    if (bRotationFollowsVelocity && Velocity.Length() > 0.01f)
    {
        FVector NormalizedVelocity = Velocity;
        NormalizedVelocity.Normalize();

        // Calculate rotation from velocity direction
        // Yaw = atan2(Y, X), Pitch = asin(Z)
        float Yaw = atan2(NormalizedVelocity.Y, NormalizedVelocity.X);
        float Pitch = asin(NormalizedVelocity.Z);

        FVector NewRotation = {Pitch, Yaw, 0.0f};
        RootComponent->SetWorldRotation(NewRotation);
    }
}

void UProjectileMovementComponent::UpdateComponentVelocity(float DeltaTime)
{
    // Apply gravity
    // 일단 표준 중력 가속도로 설정
    constexpr float GravityZ = -980.0f;
    FVector GravityAccel = FVector(0, 0, GravityZ * ProjectileGravityScale);
    Velocity += GravityAccel * DeltaTime;

    // Apply friction
    if (Friction > 0.0f)
    {
        FVector FrictionDecel = Velocity * -Friction;
        Velocity += FrictionDecel * DeltaTime;
    }

    // Clamp to max speed
    float CurrentSpeed = Velocity.Length();
    if (CurrentSpeed > MaxSpeed)
    {
        Velocity.Normalize();
        Velocity = Velocity * MaxSpeed;
    }
}

UObject* UProjectileMovementComponent::Duplicate()
{
    UProjectileMovementComponent* NewComponent = NewObject<UProjectileMovementComponent>();

    // Copy properties
    NewComponent->InitialSpeed = InitialSpeed;
    NewComponent->MaxSpeed = MaxSpeed;
    NewComponent->Velocity = Velocity;
    NewComponent->ProjectileGravityScale = ProjectileGravityScale;
    NewComponent->bRotationFollowsVelocity = bRotationFollowsVelocity;
    NewComponent->bShouldBounce = bShouldBounce;
    NewComponent->Bounciness = Bounciness;
    NewComponent->Friction = Friction;
    NewComponent->bEnabled = bEnabled;
    NewComponent->bCanEverTick = bCanEverTick;

    DuplicateSubObjects(NewComponent);

    return NewComponent;
}

void UProjectileMovementComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
    UActorComponent::DuplicateSubObjects(DuplicatedObject);
}

UClass* UProjectileMovementComponent::GetSpecificWidgetClass() const
{
    return UProjectileMovementComponentWidget::StaticClass();
}

void UProjectileMovementComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    UActorComponent::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        UE_LOG("Projectile: Serialize - LOADING");
        FJsonSerializer::ReadFloat(InOutHandle, "InitialSpeed", InitialSpeed, 1000.0f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "MaxSpeed", MaxSpeed, 3000.0f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "ProjectileGravityScale", ProjectileGravityScale,
                                   1.0f, false);
        FJsonSerializer::ReadBool(InOutHandle, "bRotationFollowsVelocity", bRotationFollowsVelocity,
                                  true, false);
        FJsonSerializer::ReadBool(InOutHandle, "bShouldBounce", bShouldBounce, false, false);
        FJsonSerializer::ReadFloat(InOutHandle, "Bounciness", Bounciness, 0.3f, false);
        FJsonSerializer::ReadFloat(InOutHandle, "Friction", Friction, 0.0f, false);
        FJsonSerializer::ReadBool(InOutHandle, "bEnabled", bEnabled, true, false);
        FJsonSerializer::ReadVector(InOutHandle, "Velocity", Velocity, FVector::ZeroVector(),
                                    false);

        // 런타임 플래그는 로드 시 초기화
        bHasBegunPlay = false;

        UE_LOG("Projectile: Serialize: Loaded: InitialSpeed=%.1f, MaxSpeed=%.1f, bEnabled=%d",
            InitialSpeed, MaxSpeed, bEnabled);
    }
    else
    {
        UE_LOG("Projectile: Serialize: SAVING: InitialSpeed=%.1f, MaxSpeed=%.1f, bEnabled=%d",
            InitialSpeed, MaxSpeed, bEnabled);
        InOutHandle["InitialSpeed"] = InitialSpeed;
        InOutHandle["MaxSpeed"] = MaxSpeed;
        InOutHandle["ProjectileGravityScale"] = ProjectileGravityScale;
        InOutHandle["bRotationFollowsVelocity"] = bRotationFollowsVelocity;
        InOutHandle["bShouldBounce"] = bShouldBounce;
        InOutHandle["Bounciness"] = Bounciness;
        InOutHandle["Friction"] = Friction;
        InOutHandle["bEnabled"] = bEnabled;
        // Velocity는 저장하지 않음
        // 런타임에 BeginPlay에서 InitialSpeed로 다시 계산
    }
}
