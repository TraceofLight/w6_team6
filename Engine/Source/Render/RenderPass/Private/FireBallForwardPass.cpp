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
    CBFireBall = FRenderResourceFactory::CreateConstantBuffer<FFireBallFwdCB>();
}

void FFireBallForwardPass::Execute(FRenderingContext& Context)
{
    if (!VS || !PS || !InputLayout || !Pipeline) { return; }

    if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
    {
        return;
    }

    if (Context.ViewMode == EViewModeIndex::VMI_Unlit)
    {
        return;
    }

    if (Context.ViewMode == EViewModeIndex::VMI_SceneDepth)
    {
        return;
    }

    ID3D11RenderTargetView* rtv = URenderer::GetInstance().GetSceneColorRTV();
    ID3D11DepthStencilView* depthReadOnly = URenderer::GetInstance().GetReadOnlyDSV();
    URenderer::GetInstance().GetDeviceContext()->OMSetRenderTargets(1, &rtv, depthReadOnly);

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

    Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

    for (UFireBallComponent* Fire : Context.FireBalls)
    {
        if (!Fire || !Fire->IsVisible()) { continue; }

         FVector CenterWS = Fire->GetWorldLocation();
         float Radius = Fire->GetRadius();
         FVector4 Color4 = Fire->GetLightColor();
         float Intensity = Fire->GetIntensity();
         float Feather = Fire->GetRadiusFallOff();

        FFireBallFwdCB cb = {};
        cb.gColor = FVector(Color4.X, Color4.Y, Color4.Z);
        cb.gIntensity = Intensity;
        cb.gCenterWS = CenterWS;
        cb.gRadius = Radius;
        cb.gFeather = Feather;
        cb.gHardness = 2.0f;
        FRenderResourceFactory::UpdateConstantBufferData(CBFireBall, cb);
        Pipeline->SetConstantBuffer(2, false, CBFireBall);

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

            //cb.gNoNormalCull = (Fire->GetOwner() == MeshComp->GetOwner()) ? 1.0f : 0.0f;
            FRenderResourceFactory::UpdateConstantBufferData(CBFireBall, cb);
            Pipeline->SetConstantBuffer(2, false, CBFireBall);

            FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, MeshComp->GetWorldTransformMatrix());
            Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

            if (MeshComp->GetIndexBuffer() && MeshComp->GetNumIndices() > 0)
            {
                Pipeline->DrawIndexed(MeshComp->GetNumIndices(), 0, 0);
            }
            else if (MeshComp->GetVertexBuffer() && MeshComp->GetNumVertices() > 0)
            {
                Pipeline->Draw(MeshComp->GetNumVertices(), 0);
            }
        }

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

            //cb.gNoNormalCull = (Fire->GetOwner() == Prim->GetOwner()) ? 1.0f : 0.0f;
            FRenderResourceFactory::UpdateConstantBufferData(CBFireBall, cb);
            Pipeline->SetConstantBuffer(2, false, CBFireBall);

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
    SafeRelease(CBFireBall);
}

