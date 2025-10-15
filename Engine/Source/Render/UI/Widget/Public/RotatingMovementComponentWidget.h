#pragma once
#include "Widget.h"

class URotatingMovementComponent;

/**
 * @brief RotatingMovementComponent의 속성을 편집하는 위젯
 * ImGui를 사용하여 회전 움직임 파라미터를 실시간으로 조정
 */
UCLASS()
class URotatingMovementComponentWidget :
    public UWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(URotatingMovementComponentWidget, UWidget)

public:
    void Initialize() override
    {
    }

    void Update() override
    {
    }

    void RenderWidget() override;

private:
    URotatingMovementComponent* FindSelectedRotatingMovementComponent() const;
};
