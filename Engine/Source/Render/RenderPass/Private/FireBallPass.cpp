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
    ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout,
    ID3D11DepthStencilState* InDepthReadState,
    ID3D11BlendState* InAdditiveBlendState)
    : FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel)
    ,VS(InVS), PS(InPS), InputLayout(InLayout)
    , DS_Read(InDepthReadState)
    , AdditiveBlend(InAdditiveBlendState)
{
    CBPerObject = FRenderResourceFactory::CreateConstantBuffer<FPerObjectCB>();
    CBFireBall = FRenderResourceFactory::CreateConstantBuffer<FFireBallCB>();
    CBInvViewProj = FRenderResourceFactory::CreateConstantBuffer<FMatrix>();
    DepthSampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_POINT, D3D11_TEXTURE_ADDRESS_CLAMP);

    D3D11_DEPTH_STENCIL_DESC d = {};
    d.DepthEnable = TRUE;
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 읽기 전용
    d.DepthFunc = D3D11_COMPARISON_ALWAYS;          // 항상 통과
    // Stencil은 필요 없으면 끔
    URenderer::GetInstance().GetDevice()->CreateDepthStencilState(&d, &DS_AlwaysRead);
}

void FFireBallPass::Execute(FRenderingContext& Context)
{
    if (!VS || !PS || !InputLayout || !Pipeline) { return; }

    auto* ctx = URenderer::GetInstance().GetDeviceContext();

    const FViewProjConstants* VP = Context.ViewProjConstants;
    const FMatrix ViewMat = VP->View;
    const FMatrix ProjMat = VP->Projection;
    const FMatrix ViewProj = ViewMat * ProjMat;

    
    ID3D11RenderTargetView* rtv = URenderer::GetInstance().GetSceneColorRTV();
    ID3D11Texture2D* depthTexture = URenderer::GetInstance().GetSceneDepthTexture();
    ID3D11DepthStencilView* depthDSV = URenderer::GetInstance().GetSceneDepthDSV();
    ID3D11DepthStencilView* depthReadOnlyDSV = URenderer::GetInstance().GetReadOnlyDSV();
    ID3D11ShaderResourceView* depthSRV = URenderer::GetInstance().GetSceneDepthSRV();

    ID3D11ShaderResourceView* nullSRV = nullptr;
    //ctx->PSSetShaderResources(0, 1, &nullSRV); // <- SceneColorSRV 쓰던 슬롯
    //ctx->VSSetShaderResources(0, 1, &nullSRV);
    //ctx->OMSetRenderTargets(1, &rtv, depthReadOnlyDSV);
    ctx->OMSetRenderTargets(1, &rtv, depthReadOnlyDSV);

    ctx->PSSetShaderResources(5, 1, &depthSRV);
    ctx->PSSetSamplers(5, 1, &DepthSampler);

    for (UFireBallComponent* Fire : Context.FireBalls)
    {
        //auto* Fire = dynamic_cast<UFireBallComponent*>(Prim);
        if (!Fire || !Fire->IsVisible()) continue;

        const FVector CenterWS = Fire->GetWorldLocation();
        const float   Radius = Fire->GetRadius();
        const FVector4 Color4 = Fire->GetLightColor();
        const float   Intensity = Fire->GetIntensity();
        const float   Feather = Fire->GetRadiusFallOff();

        // 카메라 in/out 판정
        const FVector CamWS = Context.CurrentCamera ? Context.CurrentCamera->GetLocation() : FVector::ZeroVector();
        const bool cameraInside = (CenterWS - CamWS).LengthSquared() <= Radius * Radius;

        // 라이트 볼륨 표준 스위치:
        // - 카메라 밖: Front-face Cull, DepthFunc=LESS_EQUAL (읽기전용)
        // - 카메라 안: Back-face  Cull, DepthFunc=ALWAYS     (읽기전용)
        //auto CullFrontRS = FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid });
        //auto CullBackRS = FRenderResourceFactory::GetRasterizerState({ ECullMode::Back,  EFillMode::Solid });
        auto CullFrontRS = FRenderResourceFactory::GetRasterizerState({ ECullMode::Front, EFillMode::Solid });
        auto CullBackRS = FRenderResourceFactory::GetRasterizerState({ ECullMode::Back,  EFillMode::Solid });

        ID3D11DepthStencilState* DS_LESS_EQUAL = DS_Read;         // (LESS_EQUAL, WriteOff)
        ID3D11DepthStencilState* DS_ALWAYS = DS_AlwaysRead;    // (ALWAYS,     WriteOff) ← 없으면 하나 만들어 두세요

        ID3D11RasterizerState* RS_CullNone = nullptr;
        {
            D3D11_RASTERIZER_DESC r = {};
            r.FillMode = D3D11_FILL_SOLID;
            r.CullMode = D3D11_CULL_NONE;
            r.FrontCounterClockwise = TRUE; // 상관없지만 명시
            URenderer::GetInstance().GetDevice()->CreateRasterizerState(&r, &RS_CullNone);
        }
        
        // Depth: ALWAYS + Write ZERO (깊이 비교 무시, 깊이버퍼 보존)
        ID3D11DepthStencilState* DS_DebugAlways = nullptr;
        {
            D3D11_DEPTH_STENCIL_DESC d = {};
            d.DepthEnable = TRUE;
            d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            d.DepthFunc = D3D11_COMPARISON_ALWAYS;
            URenderer::GetInstance().GetDevice()->CreateDepthStencilState(&d, &DS_DebugAlways);
        }
        
        ID3D11BlendState* BS_Opaque = nullptr;
        {
            D3D11_BLEND_DESC b = {};
            b.RenderTarget[0].BlendEnable = FALSE;                 // Opaque
            b.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            URenderer::GetInstance().GetDevice()->CreateBlendState(&b, &BS_Opaque);
        }

        FPipelineInfo Info = {
            InputLayout,
            VS,
            CullBackRS,
            DS_ALWAYS,
            //cameraInside ? CullBackRS : CullFrontRS,
            //cameraInside ? DS_ALWAYS : DS_LESS_EQUAL,
            PS,
            BS_Opaque,
            //URenderer::GetInstance().GetAdditiveBlendState()
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
        };

        FPipelineInfo dbg = {
            InputLayout,
            VS, 
            RS_CullNone,        // ← 컬링 없음
            DS_DebugAlways,     // ← 깊이 항상 통과(쓰기 OFF)
            PS,
            BS_Opaque,           // ← Opaque로 확실히 보이게
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
        };

        Pipeline->UpdatePipeline(Info);
        //Pipeline->UpdatePipeline(dbg);

        // Per-object
        FPerObjectCB POCB{};
        POCB.gWorld = FMatrix::ScaleMatrix(FVector(Radius, Radius, Radius)) * FMatrix::TranslationMatrix(CenterWS);
        POCB.gViewProj = ViewProj;
        FRenderResourceFactory::UpdateConstantBufferData(CBPerObject, POCB);
        Pipeline->SetConstantBuffer(1, true, CBPerObject);

        FMatrix InvVP = ViewProj.Inverse();
        FRenderResourceFactory::UpdateConstantBufferData(CBInvViewProj, InvVP);
        Pipeline->SetConstantBuffer(3, false, CBInvViewProj);

        // FireBall 파라미터 (b2)
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
        //auto& ViewportInfo = URenderer::GetInstance().GetViewportClient()->GetViewportInfo();

        //FBC.ViewportTopLeft = URenderer::GetInstance().GetViewportClient();
        //FBC.ViewportSize;
        //FBC.SceneRTSize;
        
        FBC.gProjRadiusNDC = (Radius * fy) / viewZ;
        FRenderResourceFactory::UpdateConstantBufferData(CBFireBall, FBC);
        Pipeline->SetConstantBuffer(2, false, CBFireBall);

        // 구 메시 Draw
        auto* VB = UAssetManager::GetInstance().GetVertexbuffer(EPrimitiveType::Sphere);
        uint32 Num = UAssetManager::GetInstance().GetNumVertices(EPrimitiveType::Sphere);
        if (VB && (Num > 0))
        {
            Pipeline->SetVertexBuffer(VB, sizeof(FNormalVertex));
            Pipeline->Draw(Num, 0);
        }
   }

   ctx->PSSetShaderResources(5, 1, &nullSRV);

   ctx->OMSetRenderTargets(1, &rtv, depthDSV);
}

void FFireBallPass::Release()
{
    SafeRelease(VS);
    SafeRelease(PS);
    SafeRelease(InputLayout);
    SafeRelease(CBPerObject);
    SafeRelease(CBFireBall);
    SafeRelease(DepthSampler);
}