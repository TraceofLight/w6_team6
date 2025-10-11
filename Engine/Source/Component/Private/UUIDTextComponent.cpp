#include "pch.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UUUIDTextComponent, UTextComponent)

/**
 * @brief Level에서 각 Actor마다 가지고 있는 UUID를 출력해주기 위한 빌보드 클래스
 * Actor has a UBillBoardComponent
 */

UUUIDTextComponent::UUUIDTextComponent() : ZOffset(0.0f) {};

UUUIDTextComponent::~UUUIDTextComponent()
{
}

void UUUIDTextComponent::OnSelected()
{
	SetVisibility(true);
}

void UUUIDTextComponent::OnDeselected()
{
	SetVisibility(false);
}

void UUUIDTextComponent::UpdateRotationMatrix(const FVector& InCameraLocation)
{
    const FVector OwnerLocation = GetOwner()->GetActorLocation();

    // 1) 먼저 최종 표시 위치 계산: 월드 Up 기준으로 오프셋을 올림
    const FVector WorldUp(0, 0, 1);
    const FVector P = OwnerLocation + WorldUp * ZOffset;

    // 2) 카메라를 향하는 Forward는 'P'를 기준으로 계산해야 함
    FVector Forward = InCameraLocation - P;
    if (Forward.LengthSquared() < 1e-8f)
    {
        Forward = FVector(0, 0, 1); // 너무 가까운 특이 케이스 방어
    }
    Forward.Normalize();

    // 3) 보조 축 계산 (월드 Up과 평행 특이점 보정)
    FVector Right = WorldUp.Cross(Forward);
    if (Right.LengthSquared() < 1e-6f)
    {
        // 카메라가 거의 정수직일 때 대체 업축 사용
        const FVector AltUp(1, 0, 0);
        Right = AltUp.Cross(Forward);
    }
    Right.Normalize();

    FVector Up = Forward.Cross(Right);
    Up.Normalize();

    // 4) 회전 + 번역(행렬 규약에 맞춰 회전 후 번역)
    FMatrix R = FMatrix(Forward, Right, Up);
    FMatrix T = FMatrix::TranslationMatrix(P);
    RTMatrix = R * T;
}

void UUUIDTextComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UTextComponent::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		GetOwner()->SetUUIDTextComponent(this);
		SetOffset(5);
	}
}

UClass* UUUIDTextComponent::GetSpecificWidgetClass() const
{
	return nullptr;
}
