#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

class FPrimitivePass : public FRenderPass
{
public:
    FPrimitivePass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel,
        ID3D11VertexShader* InVS, ID3D11PixelShader* InPS, ID3D11InputLayout* InLayout, ID3D11DepthStencilState* InDS,
        ID3D11VertexShader* InDepthVS, ID3D11PixelShader* InDepthPS, ID3D11InputLayout* InDepthLayout);
    void Execute(FRenderingContext& Context) override;
    void Release() override;

private:
    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS = nullptr;

    // Depth shaders for Scene Depth ViewMode
    ID3D11VertexShader* DepthVS = nullptr;
    ID3D11PixelShader* DepthPS = nullptr;
    ID3D11InputLayout* DepthInputLayout = nullptr;

    ID3D11Buffer* ConstantBufferColor = nullptr;
};
