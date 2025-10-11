#pragma once
#include "Widget.h"

class UDecalComponent;
class UMaterial;

UCLASS()
class UDecalComponentWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UDecalComponentWidget, UWidget)
public:
	void Initialize() override {}
	void Update() override {}
	void RenderWidget() override;

private:
	UDecalComponent* FindSelectedDecalComponent() const;
	FString GetMaterialDisplayName(UMaterial* Material) const;
};