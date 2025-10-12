#pragma once
#include "Component/Public/PrimitiveComponent.h"

UCLASS()
class UBillBoardComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UBillBoardComponent, UPrimitiveComponent)

public:
	UBillBoardComponent();
	~UBillBoardComponent();

	void FaceCamera(
		const FVector& CameraPosition,
		const FVector& CameraUp,
		const FVector& FallbackUp
	);

	const TPair<FName, ID3D11ShaderResourceView*>& GetSprite() const;
	void SetSprite(const TPair<FName, ID3D11ShaderResourceView*>& Sprite);
	void SetSprite(const UTexture* InSprite);

	ID3D11SamplerState* GetSampler() const;

	UClass* GetSpecificWidgetClass() const override;

	static const FRenderState& GetClassDefaultRenderState(); 

	void UpdateBillboardMatrix(const FVector& CameraLocation);
	FMatrix GetRTMatrix() const { return RTMatrix; }

	void SetOffset(float Offset) { ZOffset = Offset; }

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
private:
	TPair<FName, ID3D11ShaderResourceView*> Sprite = {"None", nullptr};
	ID3D11SamplerState* Sampler = nullptr;
	FMatrix RTMatrix = FMatrix::Identity();
	float ZOffset = 0.0f; // 화면상 위로 띄우는 값

public:
	virtual UObject* Duplicate() override;
protected:
	virtual void DuplicateSubObjects(UObject* DuplicatedObject) override;
};