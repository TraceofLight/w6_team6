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
    FVector RotationDeltaDegrees = RotationRate * DeltaTime;

    // Convert to radians for matrix calculations
    constexpr float DegToRad = PI / 180.0f;

    // 오버플로우 방지를 위한 각도 정규화
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

    // Get current transform
    FVector CurrentPosition = RootComponent->GetWorldLocation();
    FVector CurrentRotationDegrees = RootComponent->GetWorldRotation();

    // Calculate new rotation (in degrees for storage)
    FVector NewRotationDegrees = CurrentRotationDegrees + RotationDeltaDegrees;
    NewRotationDegrees.X = NormalizeAngle(NewRotationDegrees.X);
    NewRotationDegrees.Y = NormalizeAngle(NewRotationDegrees.Y);
    NewRotationDegrees.Z = NormalizeAngle(NewRotationDegrees.Z);

    // Convert to radians for matrix operations
    FVector CurrentRotationRadians = CurrentRotationDegrees * DegToRad;
    FVector NewRotationRadians = NewRotationDegrees * DegToRad;

    // PivotTranslation이 0이 아니면 피봇을 중심으로 회전
    if (PivotTranslation.Length() > 0.01f)
    {
        // 현재 회전 행렬로 로컬 피봇을 월드 오프셋으로 변환
        FMatrix CurrentRotMatrix = FMatrix::RotationMatrix(CurrentRotationRadians);
        FVector WorldPivotOffset = FMatrix::VectorMultiply(PivotTranslation, CurrentRotMatrix);

        // 피봇 포인트 = 액터 위치 + 피봇 오프셋
        FVector PivotPoint = CurrentPosition + WorldPivotOffset;

        // 새 회전 행렬로 피봇 오프셋 재계산
        FMatrix NewRotMatrix = FMatrix::RotationMatrix(NewRotationRadians);
        FVector NewWorldPivotOffset = FMatrix::VectorMultiply(PivotTranslation, NewRotMatrix);

        // 새 위치 = 피봇 포인트 - 새 피봇 오프셋
        FVector NewPosition = PivotPoint - NewWorldPivotOffset;

        // 위치와 회전 모두 업데이트 (Degrees로 저장)
        RootComponent->SetWorldLocation(NewPosition);
        RootComponent->SetWorldRotation(NewRotationDegrees);
    }
    else
    {
        // 피봇 없으면 단순히 제자리 회전만 (Degrees로 저장)
        RootComponent->SetWorldRotation(NewRotationDegrees);
    }
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

void URotatingMovementComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    UActorComponent::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        UE_LOG("Rotating: Serialize: LOADING");
        FJsonSerializer::ReadVector(InOutHandle, "RotationRate", RotationRate,
                                    FVector(0.0f, 90.0f, 0.0f), false);
        FJsonSerializer::ReadVector(InOutHandle, "PivotTranslation", PivotTranslation,
                                    FVector::ZeroVector(), false);
        FJsonSerializer::ReadBool(InOutHandle, "bRotateInLocalSpace", bRotateInLocalSpace, true,
                                  false);
        FJsonSerializer::ReadBool(InOutHandle, "bEnabled", bEnabled, true, false);

        // 런타임 플래그는 로드 시 초기화
        bHasBegunPlay = false;

        UE_LOG("Rotating: Serialize: Loaded: RotationRate=(%.1f, %.1f, %.1f), bEnabled=%d",
            RotationRate.X, RotationRate.Y, RotationRate.Z, bEnabled);
    }
    else
    {
        UE_LOG("Rotating: Serialize: SAVING: RotationRate=(%.1f, %.1f, %.1f), bEnabled=%d",
            RotationRate.X, RotationRate.Y, RotationRate.Z, bEnabled);
        InOutHandle["RotationRate"] = FJsonSerializer::VectorToJson(RotationRate);
        InOutHandle["PivotTranslation"] = FJsonSerializer::VectorToJson(PivotTranslation);
        InOutHandle["bRotateInLocalSpace"] = bRotateInLocalSpace;
        InOutHandle["bEnabled"] = bEnabled;
    }
}
