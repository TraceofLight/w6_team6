#pragma once
#include "Component/Public/PrimitiveComponent.h"

/**
 * @brief Orbit Component
 *
 * 원형 궤도를 라인으로 그려주는 컴포넌트
 * 궤도의 반지름과 세그먼트 수를 조절할 수 있습니다.
 *
 * @param Radius 궤도의 반지름
 * @param Segments 궤도를 구성하는 선분의 개수 (높을수록 부드러운 원)
 */
UCLASS()
class UOrbitComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UOrbitComponent, UPrimitiveComponent)

public:
	UOrbitComponent();
	~UOrbitComponent() override;

	void BeginPlay() override;
	void TickComponent() override;

	UObject* Duplicate() override;
	void Serialize(bool bInIsLoading, JSON& InOutHandle) override;

	// Widget 연결
	UClass* GetSpecificWidgetClass() const override;

	// Orbit Properties Setters
	void SetRadius(float InRadius);
	void SetSegments(int32 InSegments);
	void SetOrbitColor(const FVector4& InColor);

	// Orbit Properties Getters
	float GetRadius() const { return Radius; }
	int32 GetSegments() const { return Segments; }

	// 궤도 버텍스 재생성
	void RegenerateOrbit();

protected:
	// Orbit Parameters
	float Radius = 30.0f;
	int32 Segments = 360;

	// 동적 버텍스 데이터
	TArray<FNormalVertex> OrbitVertices;
	TArray<uint32> OrbitIndices;

	// GPU 버퍼
	ID3D11Buffer* DynamicVertexBuffer = nullptr;
	ID3D11Buffer* DynamicIndexBuffer = nullptr;

	void CreateOrbitBuffers();
	void ReleaseOrbitBuffers();

	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;
};
