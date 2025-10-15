#pragma once
#include "Actor/Public/Actor.h"

class UPointLightComponent;

/**
 * @brief Fireball Actor
 * PointLight 기반의 Actor 추가
 */
UCLASS()
class AFireballActor :
    public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(AFireballActor, AActor)

public:
    AFireballActor();
    ~AFireballActor() override = default;

    UClass* GetDefaultRootComponent() override;

    // Getter
    UPointLightComponent* GetFireballComponent() const { return FireballComponent; }

private:
    UPointLightComponent* FireballComponent = nullptr;
};
