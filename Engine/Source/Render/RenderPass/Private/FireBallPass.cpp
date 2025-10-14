#include "pch.h"
#include "Source/Render/RenderPass/Public/FireBallPass.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Component/Public/FireBallComponent.h"
#include "Editor/Public/Camera.h"


FFireBallPass::FFireBallPass(UPipeline* InPipeline,
    ID3D11Buffer* InConstantBufferViewProj,
    ID3D11Buffer* InConstantBufferModel,
    ID3D11DepthStencilState* InDepthReadState,
    ID3D11BlendState* InAdditiveBlendState)
    : FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel)
    , DS_Read(InDepthReadState)
    , AdditiveBlend(InAdditiveBlendState)
{
    // Input layout: POSITION only
    TArray<D3D11_INPUT_ELEMENT_DESC> Layout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // Use default factory entries (mainVS/mainPS)
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/FireBallShader.hlsl", Layout, &VS, &InputLayout);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/FireBallShader.hlsl", &PS);

    CBPerObject = FRenderResourceFactory::CreateConstantBuffer<FPerObjectCB>();
    CBFireBall = FRenderResourceFactory::CreateConstantBuffer<FFireBallCB>();
}

void FFireBallPass::Execute(FRenderingContext& Context)
{
    if (!VS || !PS || !InputLayout || !Pipeline) { return; }
    // Setup pipeline: depth test on (read-only), additive blend
    FPipelineInfo Info = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid }), DS_Read, PS, URenderer::GetInstance().GetAdditiveBlendState() };
    Pipeline->UpdatePipeline(Info);

    const FViewProjConstants* VP = Context.ViewProjConstants;
    const FMatrix ViewMat = VP->View;
    const FMatrix ProjMat = VP->Projection;
    const FMatrix ViewProj = ViewMat * ProjMat;

    // Iterate primitives and draw fireballs
    for (auto& Fire : Context.FireBalls)
    {
        if (!Fire || !Fire->IsVisible()) continue;

        const FVector CenterWS = Fire->GetWorldLocation();
        const float   Radius = Fire->GetRadius();
        const FVector4 Color4 = Fire->GetLightColor();
        const float   Intensity = Fire->GetIntensity();
        const float   Feather = Fire->GetRadiusFallOff();

        // Per-object matrices
        FPerObjectCB POCB{};
        POCB.gWorld = FMatrix::ScaleMatrix(FVector(Radius, Radius, Radius)) * FMatrix::TranslationMatrix(CenterWS);
        POCB.gViewProj = ViewProj;
        FRenderResourceFactory::UpdateConstantBufferData(CBPerObject, POCB);
        Pipeline->SetConstantBuffer(1, true, CBPerObject);

        // FireBall parameters (b2, PS)
        FFireBallCB FBC{};
        FBC.gColor = FVector(Color4.X, Color4.Y, Color4.Z);
        FBC.gIntensity = Intensity;
        FBC.gCenterWS = CenterWS;
        FBC.gRadius = Radius;
        FBC.gFeather = Feather;
        FBC.gHardness = 2.0f;
        const FVector4 CenterClip = FMatrix::VectorMultiply(FVector4(CenterWS.X, CenterWS.Y, CenterWS.Z, 1.0f), ViewProj);
        FBC.gCenterClip = CenterClip;
        const FVector4 CenterView = FMatrix::VectorMultiply(FVector4(CenterWS.X, CenterWS.Y, CenterWS.Z, 1.0f), ViewMat);
        const float viewZ = std::max(1e-5f, std::abs(CenterView.Z));
        const float fy = 1.0f / std::max(1e-5f, tanf((Context.CurrentCamera ? Context.CurrentCamera->GetFovY() : 1.1f) * 0.5f));
        FBC.gProjRadiusNDC = (Radius * fy) / viewZ;
        FRenderResourceFactory::UpdateConstantBufferData(CBFireBall, FBC);
        Pipeline->SetConstantBuffer(2, false, CBFireBall);

        // Bind sphere vertex buffer and draw
        auto* VB = UAssetManager::GetInstance().GetVertexbuffer(EPrimitiveType::Sphere);
        uint32 Num = UAssetManager::GetInstance().GetNumVertices(EPrimitiveType::Sphere);
        if (VB && Num > 0) { Pipeline->SetVertexBuffer(VB, sizeof(FNormalVertex)); Pipeline->Draw(Num, 0); }
    }
}

void FFireBallPass::Release()
{
    SafeRelease(VS);
    SafeRelease(PS);
    SafeRelease(InputLayout);
    SafeRelease(CBPerObject);
    SafeRelease(CBFireBall);
}