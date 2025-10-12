#include "pch.h"
#include "Render/RenderPass/Public/DecalPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/RenderPass/Public/RenderingContext.h"
#include "Component/Public/DecalComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Physics/Public/OBB.h"
#include "Texture/Public/Texture.h"
#include "Texture/Public/TextureRenderProxy.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"

FDecalPass::FDecalPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS_Read, ID3D11BlendState* InBlendState)
    : FRenderPass(InPipeline, InConstantBufferViewProj, nullptr),
    VS(InVS), PS(InPS), InputLayout(InLayout), DS_Read(InDS_Read), BlendState(InBlendState)
{
    ConstantBufferPrim = FRenderResourceFactory::CreateConstantBuffer<FModelConstants>();
    ConstantBufferDecal = FRenderResourceFactory::CreateConstantBuffer<FDecalConstants>();
}

void FDecalPass::Execute(FRenderingContext& Context)
{
    // 플래그가 꺼져 있으면 전체 스킵
    if (!(Context.ShowFlags & EEngineShowFlags::SF_Decal)) { return; }
    if (Context.Decals.empty()) { return; }

    // --- Set Pipeline State ---
    FPipelineInfo PipelineInfo = { InputLayout, VS, FRenderResourceFactory::GetRasterizerState({ ECullMode::Back, EFillMode::Solid }),
        DS_Read, PS, BlendState };
    Pipeline->UpdatePipeline(PipelineInfo);
    Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

    // --- Render Decals ---
    for (UDecalComponent* Decal : Context.Decals)
    {
        if (!Decal || !Decal->IsVisible()) { continue; }

        const IBoundingVolume* DecalBV = Decal->GetBoundingBox();
        if (!DecalBV || DecalBV->GetType() != EBoundingVolumeType::OBB) { continue; }

        const FOBB* DecalOBB = static_cast<const FOBB*>(DecalBV);

        // --- Update Decal Constant Buffer ---
        FDecalConstants DecalConstants;

        // 데칼의 사이즈를 추가적으로 곱해주기 위해
        FVector Size = Decal->GetDecalSize();
        const float Eps = 1e-4f;
        Size.X = std::max(Size.X, Eps);
        Size.Y = std::max(Size.Y, Eps);
        Size.Z = std::max(Size.Z, Eps);

        const FMatrix Scale = FMatrix::ScaleMatrix(Size);
        const FMatrix ScaleInv = FMatrix::ScaleMatrixInverse(Size);

        DecalConstants.DecalWorld = Scale * Decal->GetWorldTransformMatrix();
        DecalConstants.DecalWorldInverse = Decal->GetWorldTransformMatrixInverse() * ScaleInv;
        DecalConstants.SpotAngle = Decal->GetSpotAngle();
        DecalConstants.BlendFactor = Decal->GetBlendFactor();
        DecalConstants.Padding2 = 0.0f;
        DecalConstants.Padding3 = 0.0f;
        DecalConstants.DecalFadeParams = FVector4(Decal->GetFadeAlpha(), 0, 0, 0);

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferDecal, DecalConstants);
        Pipeline->SetConstantBuffer(2, false, ConstantBufferDecal);

        // --- Bind Decal Texture ---
        UTexture* BoundTexture = nullptr;
        if (UMaterial* Mat = Decal->GetMaterial())
        {
            // 알파 먼저(데칼 컷아웃에 유용), 없으면 디퓨즈, 없으면 앰비언트
            BoundTexture = Mat->GetAlphaTexture();
            if (!BoundTexture) BoundTexture = Mat->GetDiffuseTexture();
            if (!BoundTexture) BoundTexture = Mat->GetAmbientTexture();
        }
        if (!BoundTexture)
        {
            BoundTexture = Decal->GetTexture(); // 최후 폴백
        }

        if (BoundTexture)
        {
            if (auto* Proxy = BoundTexture->GetRenderProxy())
            {
                Pipeline->SetTexture(0, false, Proxy->GetSRV());
                Pipeline->SetSamplerState(0, false, Proxy->GetSampler());
            }
        }
        // 1) 기존 기본 프리미티브
        for (UPrimitiveComponent* Prim : Context.DefaultPrimitives)
        {
            DrawDecalReceiver(Prim);
        }

        // 2) StaticMesh도 수신자로 포함
        for (UStaticMeshComponent* SM : Context.StaticMeshes)
        {
            DrawDecalReceiver(SM);
        }
    }
}

void FDecalPass::DrawDecalReceiver(UPrimitiveComponent* Prim)
{
    if (!Prim || !Prim->IsVisible()) return;

    FModelConstants ModelConstants{
        Prim->GetWorldTransformMatrix(),
        Prim->GetWorldTransformMatrixInverse().Transpose()
    };
    FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPrim, ModelConstants);
    Pipeline->SetConstantBuffer(0, true, ConstantBufferPrim);

    Pipeline->SetVertexBuffer(Prim->GetVertexBuffer(), sizeof(FNormalVertex));
    if (Prim->GetIndexBuffer() && Prim->GetIndicesData())
    {
        Pipeline->SetIndexBuffer(Prim->GetIndexBuffer(), 0);
        Pipeline->DrawIndexed(Prim->GetNumIndices(), 0, 0);
    }
    else
    {
        Pipeline->Draw(Prim->GetNumVertices(), 0);
    }

}
void FDecalPass::Release()
{
    SafeRelease(ConstantBufferPrim);
    SafeRelease(ConstantBufferDecal);
}