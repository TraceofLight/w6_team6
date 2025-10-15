#include "pch.h"
#include "Render/RenderPass/Public/FireBallForwardPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Public/FireBallComponent.h"

FFireBallForwardPass::FFireBallForwardPass(UPipeline* InPipeline,
    ID3D11Buffer* InConstantBufferViewProj,
    ID3D11Buffer* InConstantBufferModel,
    ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout,
    ID3D11DepthStencilState* InDepthReadState,
    ID3D11BlendState* InAdditiveBlendState)
    : FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel)
    , VS(InVS), PS(InPS), InputLayout(InLayout)
    , DS_Read(InDepthReadState)
    , AdditiveBlend(InAdditiveBlendState)
{
    CBFireBall = FRenderResourceFactory::CreateConstantBuffer<FFireBallCB>();
}

void FFireBallForwardPass::Execute(FRenderingContext& Context)
{
    if (!VS || !PS || !InputLayout || !Pipeline) { return; }

    // Bind Scene RT with read-only depth so we can read-test depth
    ID3D11RenderTargetView* rtv = URenderer::GetInstance().GetSceneColorRTV();
    ID3D11DepthStencilView* depthReadOnly = URenderer::GetInstance().GetReadOnlyDSV();
    URenderer::GetInstance().GetDeviceContext()->OMSetRenderTargets(1, &rtv, depthReadOnly);

    // Pipeline state: additive blend, depth test (read-only)
    FPipelineInfo pipelineInfo = {
        InputLayout,
        VS,
        FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),
        DS_Read,
        PS,
        AdditiveBlend,
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    };
    Pipeline->UpdatePipeline(pipelineInfo);

    // Set shared constant buffers
    Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

    // For each fireball light
    for (UFireBallComponent* Fire : Context.FireBalls)
    {
        if (!Fire || !Fire->IsVisible()) { continue; }

        const FVector CenterWS = Fire->GetWorldLocation();
        const float Radius = Fire->GetRadius();
        const FVector4 Color4 = Fire->GetLightColor();
        const float Intensity = Fire->GetIntensity();
        const float Feather = Fire->GetRadiusFallOff();

        FFireBallCB cb = {};
        cb.gColor = FVector(Color4.X, Color4.Y, Color4.Z);
        cb.gIntensity = Intensity;
        cb.gCenterWS = CenterWS;
        cb.gRadius = Radius;
        cb.gFeather = Feather;
        cb.gHardness = 2.0f;
        FRenderResourceFactory::UpdateConstantBufferData(CBFireBall, cb);
        Pipeline->SetConstantBuffer(2, false, CBFireBall);

        // Light all static meshes
        for (UStaticMeshComponent* MeshComp : Context.StaticMeshes)
        {
            if (!MeshComp || !MeshComp->GetStaticMesh()) { continue; }
            if (ID3D11Buffer* vb = MeshComp->GetVertexBuffer())
            {
                Pipeline->SetVertexBuffer(vb, sizeof(FNormalVertex));
            }
            if (ID3D11Buffer* ib = MeshComp->GetIndexBuffer())
            {
                Pipeline->SetIndexBuffer(ib, sizeof(uint32));
            }

            // Per-object model matrix
            FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, MeshComp->GetWorldTransformMatrix());
            Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

            // Draw - prefer indexed when available
            if (MeshComp->GetIndexBuffer() && MeshComp->GetNumIndices() > 0)
            {
                Pipeline->DrawIndexed(MeshComp->GetNumIndices(), 0, 0);
            }
            else if (MeshComp->GetVertexBuffer() && MeshComp->GetNumVertices() > 0)
            {
                Pipeline->Draw(MeshComp->GetNumVertices(), 0);
            }
        }

        // Light default primitives (non-static meshes)
        for (UPrimitiveComponent* Prim : Context.DefaultPrimitives)
        {
            if (!Prim || !Prim->IsVisible()) { continue; }

            if (ID3D11Buffer* vb = Prim->GetVertexBuffer())
            {
                Pipeline->SetVertexBuffer(vb, sizeof(FNormalVertex));
            }
            if (ID3D11Buffer* ib = Prim->GetIndexBuffer())
            {
                Pipeline->SetIndexBuffer(ib, sizeof(uint32));
            }

            FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, Prim->GetWorldTransformMatrix());
            Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

            if (Prim->GetIndexBuffer() && Prim->GetNumIndices() > 0)
            {
                Pipeline->DrawIndexed(Prim->GetNumIndices(), 0, 0);
            }
            else if (Prim->GetVertexBuffer() && Prim->GetNumVertices() > 0)
            {
                Pipeline->Draw(Prim->GetNumVertices(), 0);
            }
        }
    }
}

void FFireBallForwardPass::Release()
{
    // External VS/PS/InputLayout managed by renderer create/release
    SafeRelease(CBFireBall);
}

