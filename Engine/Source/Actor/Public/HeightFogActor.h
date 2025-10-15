#pragma once

class UHeightFogComponent;
UCLASS()
class AHeightFogActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(AHeightFogActor, AActor)

public:
	AHeightFogActor();
	UClass* GetDefaultRootComponent() override;

private:
	UHeightFogComponent* HeightFogComponent = nullptr;
};
