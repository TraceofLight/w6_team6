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

    // Resolve pending UpdatedComponent if duplication set a name
    ResolvePendingUpdatedComponent();
}

void URotatingMovementComponent::SetUpdatedComponent(USceneComponent* InComponent)
{
    UpdatedComponent = InComponent;
}

void URotatingMovementComponent::TickComponent()
{
    if (!bEnabled)
    {
        return;
    }

    if (!bHasBegunPlay)
    {
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

    // Get the component to rotate - use UpdatedComponent if set, otherwise use RootComponent
    USceneComponent* ComponentToRotate = UpdatedComponent;
    if (!ComponentToRotate)
    {
        ComponentToRotate = OwnerActor->GetRootComponent();
    }

    if (!ComponentToRotate)
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

    // UpdatedComponent가 설정되어 있으면 RelativeTransform을 회전
    if (UpdatedComponent)
    {
        // Get current relative rotation
        FVector CurrentRotationDegrees = ComponentToRotate->GetRelativeRotation();

        // Calculate new rotation (in degrees for storage)
        FVector NewRotationDegrees = CurrentRotationDegrees + RotationDeltaDegrees;
        NewRotationDegrees.X = NormalizeAngle(NewRotationDegrees.X);
        NewRotationDegrees.Y = NormalizeAngle(NewRotationDegrees.Y);
        NewRotationDegrees.Z = NormalizeAngle(NewRotationDegrees.Z);

        // Update relative rotation
        ComponentToRotate->SetRelativeRotation(NewRotationDegrees);
    }
    else
    {
        // 기존 방식: World Transform 회전
        // Get current transform
        FVector CurrentPosition = ComponentToRotate->GetWorldLocation();
        FVector CurrentRotationDegrees = ComponentToRotate->GetWorldRotation();

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
            ComponentToRotate->SetWorldLocation(NewPosition);
            ComponentToRotate->SetWorldRotation(NewRotationDegrees);
        }
        else
        {
            // 피봇 없으면 단순히 제자리 회전만 (Degrees로 저장)
            ComponentToRotate->SetWorldRotation(NewRotationDegrees);
        }
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

    // Store UpdatedComponent class and index for later resolution
    if (UpdatedComponent && GetOwner())
    {
        // Store the class name
        NewComponent->PendingUpdatedComponentClassName = UpdatedComponent->GetClass()->GetName().ToString();

        // Find the index of this component among components of the same type
        int32 Index = 0;
        for (UActorComponent* Comp : GetOwner()->GetOwnedComponents())
        {
            if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
            {
                if (SceneComp->GetClass()->GetName().ToString() == NewComponent->PendingUpdatedComponentClassName)
                {
                    if (SceneComp == UpdatedComponent)
                    {
                        NewComponent->PendingUpdatedComponentIndex = Index;
                        break;
                    }
                    Index++;
                }
            }
        }
    }
    else
    {
        NewComponent->PendingUpdatedComponentClassName = "";
        NewComponent->PendingUpdatedComponentIndex = -1;
    }

    DuplicateSubObjects(NewComponent);

    return NewComponent;
}

void URotatingMovementComponent::ResolvePendingUpdatedComponent()
{
    if (PendingUpdatedComponentClassName.empty() || PendingUpdatedComponentIndex < 0 || !GetOwner())
    {
        return;
    }

    // Find component by class and index
    int32 CurrentIndex = 0;
    for (UActorComponent* Comp : GetOwner()->GetOwnedComponents())
    {
        if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
        {
            std::string ComponentClassName = SceneComp->GetClass()->GetName().ToString();

            // Check if this component matches the target class
            if (ComponentClassName == PendingUpdatedComponentClassName)
            {
                if (CurrentIndex == PendingUpdatedComponentIndex)
                {
                    UpdatedComponent = SceneComp;
                    PendingUpdatedComponentClassName = "";
                    PendingUpdatedComponentIndex = -1;
                    return;
                }

                CurrentIndex++;
            }
        }
    }

    PendingUpdatedComponentClassName = "";
    PendingUpdatedComponentIndex = -1;
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
        FJsonSerializer::ReadVector(InOutHandle, "RotationRate", RotationRate,
                                    FVector(0.0f, 90.0f, 0.0f), false);
        FJsonSerializer::ReadVector(InOutHandle, "PivotTranslation", PivotTranslation,
                                    FVector::ZeroVector(), false);
        FJsonSerializer::ReadBool(InOutHandle, "bRotateInLocalSpace", bRotateInLocalSpace, true,
                                  false);
        FJsonSerializer::ReadBool(InOutHandle, "bEnabled", bEnabled, true, false);

        // Load UpdatedComponent by class and index
        std::string UpdatedComponentClassName;
        int32 UpdatedComponentIndex = -1;

        FJsonSerializer::ReadString(InOutHandle, "UpdatedComponentClassName", UpdatedComponentClassName, "", false);
        FJsonSerializer::ReadInt32(InOutHandle, "UpdatedComponentIndex", UpdatedComponentIndex, -1, false);

        if (!UpdatedComponentClassName.empty() && UpdatedComponentIndex >= 0 && GetOwner())
        {
            // Find component by class and index
            int32 CurrentIndex = 0;
            for (UActorComponent* Comp : GetOwner()->GetOwnedComponents())
            {
                if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
                {
                    std::string ComponentClassName = SceneComp->GetClass()->GetName().ToString();

                    if (ComponentClassName == UpdatedComponentClassName)
                    {
                        if (CurrentIndex == UpdatedComponentIndex)
                        {
                            UpdatedComponent = SceneComp;
                            break;
                        }
                        CurrentIndex++;
                    }
                }
            }

            if (!UpdatedComponent)
            {
                // Store for later resolution in BeginPlay
                PendingUpdatedComponentClassName = UpdatedComponentClassName;
                PendingUpdatedComponentIndex = UpdatedComponentIndex;
            }
        }
        else if (!UpdatedComponentClassName.empty())
        {
            // Store for later resolution in BeginPlay
            PendingUpdatedComponentClassName = UpdatedComponentClassName;
            PendingUpdatedComponentIndex = UpdatedComponentIndex;
        }

        // 런타임 플래그는 로드 시 초기화
        bHasBegunPlay = false;
    }
    else
    {
        InOutHandle["RotationRate"] = FJsonSerializer::VectorToJson(RotationRate);
        InOutHandle["PivotTranslation"] = FJsonSerializer::VectorToJson(PivotTranslation);
        InOutHandle["bRotateInLocalSpace"] = bRotateInLocalSpace;
        InOutHandle["bEnabled"] = bEnabled;

        // Save UpdatedComponent by class and index
        if (UpdatedComponent && GetOwner())
        {
            // Get class name
            std::string ClassName = UpdatedComponent->GetClass()->GetName().ToString();
            InOutHandle["UpdatedComponentClassName"] = ClassName;

            // Find index among components of the same type
            int32 Index = 0;
            for (UActorComponent* Comp : GetOwner()->GetOwnedComponents())
            {
                if (USceneComponent* SceneComp = Cast<USceneComponent>(Comp))
                {
                    std::string CompClassName = SceneComp->GetClass()->GetName().ToString();

                    if (CompClassName == ClassName)
                    {
                        if (SceneComp == UpdatedComponent)
                        {
                            InOutHandle["UpdatedComponentIndex"] = Index;
                            break;
                        }
                        Index++;
                    }
                }
            }
        }
        else
        {
            InOutHandle["UpdatedComponentClassName"] = "";
            InOutHandle["UpdatedComponentIndex"] = -1;
        }
    }
}
