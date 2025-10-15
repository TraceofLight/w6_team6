#pragma once
#include "Widget.h"

class UProjectileMovementComponent;

/**
 * @brief ProjectileMovementComponent의 속성을 편집하는 위젯
 * ImGui를 사용하여 발사체 움직임 파라미터를 실시간으로 조정
 */
UCLASS()
class UProjectileMovementComponentWidget :
    public UWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UProjectileMovementComponentWidget, UWidget)

public:
    void Initialize() override
    {
    }

    void Update() override
    {
    }

    void RenderWidget() override;

private:
    UProjectileMovementComponent* FindSelectedProjectileMovementComponent() const;
};
