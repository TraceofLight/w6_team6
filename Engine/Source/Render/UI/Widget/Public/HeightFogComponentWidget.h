#pragma once
#include "Widget.h"

class UHeightFogComponent;

/**
 * @brief HeightFogComponent의 프로퍼티를 편집하는 위젯
 *
 * ImGui를 사용하여 안개 파라미터를 실시간으로 조정
 */
UCLASS()
class UHeightFogComponentWidget : public UWidget
{
    GENERATED_BODY()
    DECLARE_CLASS(UHeightFogComponentWidget, UWidget)

public:
    void Initialize() override {}
    void Update() override {}
    void RenderWidget() override;

private:
    UHeightFogComponent* FindSelectedHeightFogComponent() const;
};
