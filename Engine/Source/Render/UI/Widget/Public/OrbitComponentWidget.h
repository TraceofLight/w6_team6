#pragma once
#include "Widget.h"

class UOrbitComponent;

UCLASS()
class UOrbitComponentWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UOrbitComponentWidget, UWidget)

public:
	void Initialize() override {}
	void Update() override {}
	void RenderWidget() override;

private:
	UOrbitComponent* FindSelectedOrbitComponent() const;
};
