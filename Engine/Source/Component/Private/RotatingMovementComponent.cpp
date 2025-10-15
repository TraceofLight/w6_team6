#include "pch.h"
#include "Component/Public/RotatingMovementComponent.h"
#include "Actor/Public/Actor.h"
#include "Component/Public/SceneComponent.h"
#include "Render/UI/Widget/Public/RotatingMovementComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(URotatingMovementComponent, UActorComponent)

URotatingMovementComponent::URotatingMovementComponent()
{
    ComponentType = EComponentType::Actor;
    bCanEverTick = true;
}

URotatingMovementComponent::~URotatingMovementComponent() = default;

void URotatingMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    bHasBegunPlay = true;

    UE_LOG("Rotating: BeginPlay: Owner: %s, RotationRate: (%.1f, %.1f, %.1f)",
           GetOwner() ? GetOwner()->GetName().ToString().data() : "nullptr",
           RotationRate.X, RotationRate.Y, RotationRate.Z);
}

void URotatingMovementComponent::TickComponent()
{
    if (!bEnabled)
    {
        return;
    }

    if (!bHasBegunPlay)
    {
        UE_LOG_WARNING("Rotating: TickComponent: BeginPlay not called yet!");
        return;
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return;
    }

    float DeltaTime = DT;
    if (DeltaTime <= 0.0f)
    {
        return;
    }

    static int TickCount = 0;
    if (TickCount++ < 5)
    {
        UE_LOG(
            "Rotating: TickComponent: DeltaTime: %.3f, RotationRate: (%.1f, %.1f, %.1f)",
            DeltaTime, RotationRate.X, RotationRate.Y, RotationRate.Z);
    }

    // Get root component to update rotation
    USceneComponent* RootComponent = OwnerActor->GetRootComponent();
    if (!RootComponent)
    {
        return;
    }

    // Calculate rotation delta (degrees to apply this frame)
    FVector RotationDelta = RotationRate * DeltaTime;

    // Get current rotation
    FVector CurrentRotation = RootComponent->GetWorldRotation();
    FVector NewRotation = CurrentRotation + RotationDelta;

    // 오버플로우 방지를 위한 각도 clamping
    auto NormalizeAngle = [](float Angle) -> float
    {
        // Normalize to -180 ~ 180 range
        while (Angle > 180.0f)
        {
            Angle -= 360.0f;
        }
        while (Angle < -180.0f)
        {
            Angle += 360.0f;
        }
        return Angle;
    };

    NewRotation.X = NormalizeAngle(NewRotation.X);
    NewRotation.Y = NormalizeAngle(NewRotation.Y);
    NewRotation.Z = NormalizeAngle(NewRotation.Z);

    RootComponent->SetWorldRotation(NewRotation);
}

UObject* URotatingMovementComponent::Duplicate()
{
    URotatingMovementComponent* NewComponent = NewObject<URotatingMovementComponent>();

    // Copy properties
    NewComponent->RotationRate = RotationRate;
    NewComponent->PivotTranslation = PivotTranslation;
    NewComponent->bRotateInLocalSpace = bRotateInLocalSpace;
    NewComponent->bEnabled = bEnabled;
    NewComponent->bCanEverTick = bCanEverTick;

    DuplicateSubObjects(NewComponent);

    return NewComponent;
}

void URotatingMovementComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
    UActorComponent::DuplicateSubObjects(DuplicatedObject);
}

UClass* URotatingMovementComponent::GetSpecificWidgetClass() const
{
    return URotatingMovementComponentWidget::StaticClass();
}

void URotatingMovementComponent::Serialize(bool bInIsLoading, JSON& InOutHandle)
{
    UActorComponent::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        FJsonSerializer::ReadVector(InOutHandle, "RotationRate", RotationRate,
                                    FVector(0.0f, 90.0f, 0.0f), false);
        FJsonSerializer::ReadVector(InOutHandle, "PivotTranslation", PivotTranslation,
                                    FVector::ZeroVector(), false);
        FJsonSerializer::ReadBool(InOutHandle, "bRotateInLocalSpace", bRotateInLocalSpace, true,
                                  false);
        FJsonSerializer::ReadBool(InOutHandle, "bEnabled", bEnabled, true, false);
    }
    else
    {
        InOutHandle["RotationRate"] = FJsonSerializer::VectorToJson(RotationRate);
        InOutHandle["PivotTranslation"] = FJsonSerializer::VectorToJson(PivotTranslation);
        InOutHandle["bRotateInLocalSpace"] = bRotateInLocalSpace;
        InOutHandle["bEnabled"] = bEnabled;
    }
}
