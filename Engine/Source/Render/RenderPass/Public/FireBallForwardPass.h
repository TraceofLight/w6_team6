#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

// Dedicated forward-pass constant buffer matching FireBallForward.hlsl layout
struct FFireBallFwdCB
{
    FVector gColor;   float gIntensity;
    FVector gCenterWS; float gRadius;
    FVector4 gCenterClip; // kept for layout parity
    float gProjRadiusNDC; float gFeather; float gHardness; float _pad;
};

class FFireBallForwardPass : public FRenderPass
{
public:
    FFireBallForwardPass(UPipeline* InPipeline,
        ID3D11Buffer* InConstantBufferViewProj,
        ID3D11Buffer* InConstantBufferModel,
        ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout,
        ID3D11DepthStencilState* InDepthReadState,
        ID3D11BlendState* InAdditiveBlendState);

    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS_Read = nullptr;
    ID3D11BlendState* AdditiveBlend = nullptr;

    ID3D11Buffer* CBFireBall = nullptr;
};

