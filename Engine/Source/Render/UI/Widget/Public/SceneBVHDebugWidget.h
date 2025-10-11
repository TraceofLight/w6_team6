#pragma once
#include "Widget.h"

/**
 * @brief SceneBVH 디버그 제어 위젯
 */
class USceneBVHDebugWidget : public UWidget
{
public:
	USceneBVHDebugWidget();
	virtual ~USceneBVHDebugWidget() override {}

	void RenderWidget() override;

private:
	bool bShowBVH = false;
	int MaxDepth = -1;
};
