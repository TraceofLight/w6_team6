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

    // 공통: RTV + DSV 바인딩 (깊이 테스트 활성)
    ID3D11RenderTargetView* rtv = URenderer::GetInstance().GetDeviceResources()->GetSceneColorRTV();
    ID3D11DepthStencilView* dsv = URenderer::GetInstance().GetSceneDepthDSV(); // ← DSV!
    ID3D11Texture2D* LinearDepth = URenderer::GetInstance().GetSceneDepthTexture();
    //ctx->OMSetRenderTargets(1, &rtv, dsv);

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
        auto CullFrontRS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid });
        auto CullBackRS = FRenderResourceFactory::GetRasterizerState({ ECullMode::None,  EFillMode::Solid });

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
            d.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
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
            BS_Opaque
            //URenderer::GetInstance().GetAdditiveBlendState()
        };

        //FPipelineInfo dbg = {
        //    InputLayout,
        //    VS,
        //    RS_CullNone,        // ← 컬링 없음
        //    DS_DebugAlways,     // ← 깊이 항상 통과(쓰기 OFF)
        //    PS,
        //    BS_Opaque           // ← Opaque로 확실히 보이게
        //};

        Pipeline->UpdatePipeline(Info);
        //Pipeline->UpdatePipeline(dbg);

        // Per-object
        FPerObjectCB POCB{};
        POCB.gWorld = FMatrix::ScaleMatrix(FVector(Radius, Radius, Radius)) * FMatrix::TranslationMatrix(CenterWS);
        POCB.gViewProj = ViewProj;
        FRenderResourceFactory::UpdateConstantBufferData(CBPerObject, POCB);
        Pipeline->SetConstantBuffer(1, true, CBPerObject);

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
        FBC.gProjRadiusNDC = (Radius * fy) / viewZ;
        FRenderResourceFactory::UpdateConstantBufferData(CBFireBall, FBC);
        Pipeline->SetConstantBuffer(2, false, CBFireBall);

        // (D3D11.0 경로) 깊이 SRV는 바인딩하지 않음 — 같은 리소스를 DSV와 SRV로 동시 바인딩 불가
        // ctx->PSSetShaderResources(5, 1, &depthSRV);    // X
        // ctx->PSSetSamplers(5, 1, &DepthSampler);       // X

        // 구 메시 Draw
        auto* VB = UAssetManager::GetInstance().GetVertexbuffer(EPrimitiveType::Sphere);
        uint32 Num = UAssetManager::GetInstance().GetNumVertices(EPrimitiveType::Sphere);
        if (VB && Num > 0) {
            Pipeline->SetVertexBuffer(VB, sizeof(FNormalVertex));
            Pipeline->Draw(Num, 0);
        }
    }

    // (디버그) t5 언바인드 - 이번 경로에선 바인딩 안 했지만, 혹시 남아있을 수도 있으니 안전하게
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(5, 1, &nullSRV);
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
