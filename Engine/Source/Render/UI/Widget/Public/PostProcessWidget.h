#pragma once
#include "Widget.h"

class UPostProcessWidget : public UWidget
{
public:
    UPostProcessWidget() : UWidget("PostProcess Widget") {}
    ~UPostProcessWidget() override = default;

    void Initialize() override {}
    void Update() override {}
    void RenderWidget() override;
};