#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

struct FPerObjectCB
{
    FMatrix gWorld;
    FMatrix gViewProj;
};

struct FFireBallCB
{
    FVector gColor;   float gIntensity;
    FVector gCenterWS; float gRadius;
    FVector4 gCenterClip; // unused
    FVector2 ViewportTopLeft;
    FVector2 ViewportSize;
    FVector2 SceneRTSize;
    float gProjRadiusNDC; float gFeather; float gHardness;
    FVector _pad;
};

class FFireBallPass : public FRenderPass
{
public:
    FFireBallPass(UPipeline* InPipeline,
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
    ID3D11DepthStencilState* DS_AlwaysRead = nullptr;
    ID3D11DepthStencilState* DS_GreaterEqual = nullptr;
    ID3D11BlendState* AdditiveBlend = nullptr;

    ID3D11Buffer* CBPerObject = nullptr; // b1: gWorld, gViewProj
    ID3D11Buffer* CBFireBall = nullptr; // b2: params
    ID3D11Buffer* CBInvViewProj = nullptr; // b3: inverse ViewProj
    ID3D11SamplerState* DepthSampler = nullptr;
};
