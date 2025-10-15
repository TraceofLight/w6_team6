#pragma once
#include "Widget.h"

class UPointLightComponent;

/**
 * @brief FireballComponent의 속성을 편집하는 위젯
 * ImGui를 사용하여 Point Light 파라미터를 실시간으로 조정
 */
UCLASS()
class UPointLightComponentWidget :
    public UWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UPointLightComponentWidget, UWidget)

public:
    void Initialize() override
    {
    }

    void Update() override
    {
    }

    void RenderWidget() override;

private:
    UPointLightComponent* FindSelectedFireballComponent() const;
};
