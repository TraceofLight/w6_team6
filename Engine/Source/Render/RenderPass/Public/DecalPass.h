#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

// Matches the layout in DecalShader.hlsl
struct FModelConstants
{
    FMatrix World;
    FMatrix WorldInverseTranspose;
};

struct FDecalConstants
{
    FMatrix DecalWorld;
    FMatrix DecalWorldInverse;
    FVector4 DecalFadeParams; // x = FadeAlpha, YZW = padding
    float SpotAngle;           // 원뿔 각도 (degree). < 0이면 박스 클리핑, >= 0이면 원뿔 프러스텀
    float BlendFactor;         // 블렌딩 강도 (0.0 ~ 1.0)
    float Padding2;
    float Padding3;
};

class FDecalPass : public FRenderPass
{
public:
    FDecalPass(
        UPipeline* InPipeline,
        ID3D11Buffer* InConstantBufferViewProj,
        ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS_Read, ID3D11BlendState* InBlendState
    );
    void Execute(FRenderingContext& Context) override;
    void DrawDecalReceiver(UPrimitiveComponent* Prim);
    void Release() override;

private:
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS_Read = nullptr;
    ID3D11BlendState* BlendState = nullptr;

    ID3D11Buffer* ConstantBufferDecal = nullptr;
    ID3D11Buffer* ConstantBufferPrim = nullptr;
};