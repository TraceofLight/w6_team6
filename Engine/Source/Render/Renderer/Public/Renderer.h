#pragma once
#include "DeviceResources.h"
#include "Core/Public/Object.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Component/Public/BillBoardComponent.h"
#include "Component/Public/TextComponent.h"

class UDeviceResources;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UUUIDTextComponent;
class AActor;
class AGizmo;
class UEditor;
class UFontRenderer;
class FViewport;
class UCamera;
class UPipeline;

struct FFXAAParameters
{
	float SubpixelBlend = 0.3f;
	float EdgeThreshold = 0.20f;
	float EdgeThresholdMin = 0.05f;
	float Padding = 0.0f;
};

/**
 * @brief Rendering Pipeline 전반을 처리하는 클래스
 */
UCLASS()
class URenderer : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(URenderer, UObject)

public:
	void Init(HWND InWindowHandle);
	void Release();

	// Initialize
	void CreateDepthStencilState();
	void CreateBlendState();
	void CreateDefaultShader();
	void CreateDecalShader();
	void CreateTextureShader();
	void CreateDepthShader();
	void CreateConstantBuffers();

	// Release
	void ReleaseConstantBuffers();
	void ReleaseDefaultShader();
	void ReleaseDepthStencilState();
	void ReleaseBlendState();
	void ReleaseDepthShader();

	// Render
	void Update();
	void RenderBegin() const;
	void RenderLevel(UCamera* InCurrentCamera);
	void RenderEnd() const;
	void RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride = 0, uint32 InIndexBufferStride = 0);

	void OnResize(uint32 Inwidth = 0, uint32 InHeight = 0) const;

	// Getter & Setter
	ID3D11Device* GetDevice() const { return DeviceResources->GetDevice(); }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceResources->GetDeviceContext(); }
	IDXGISwapChain* GetSwapChain() const { return DeviceResources->GetSwapChain(); }
	ID3D11RenderTargetView* GetRenderTargetView() const { return DeviceResources->GetRenderTargetView(); }
	UDeviceResources* GetDeviceResources() const { return DeviceResources; }
	FViewport* GetViewportClient() const { return ViewportClient; }
	UPipeline* GetPipeline() const { return Pipeline; }
	bool GetIsResizing() const { return bIsResizing; }

	ID3D11DepthStencilState* GetDefaultDepthStencilState() const { return DefaultDepthStencilState; }
	ID3D11DepthStencilState* GetDisabledDepthStencilState() const { return DisabledDepthStencilState; }
	ID3D11BlendState* GetAlphaBlendState() const { return AlphaBlendState; }
	ID3D11BlendState* GetAdditiveBlendState() const { return AdditiveBlendState; }
	ID3D11Buffer* GetConstantBufferModels() const { return ConstantBufferModels; }
	ID3D11Buffer* GetConstantBufferViewProj() const { return ConstantBufferViewProj; }

	ID3D11VertexShader* GetDepthVertexShader() const { return DepthVertexShader; }
	ID3D11PixelShader* GetDepthPixelShader() const { return DepthPixelShader; }
	ID3D11InputLayout* GetDepthInputLayout() const { return DepthInputLayout; }

	void SetIsResizing(bool isResizing) { bIsResizing = isResizing; }

	void SetFXAAEnabled(bool bEnabled) { bIsFXAAEnabled = bEnabled; }
	bool IsFXAAEnabled() const { return bIsFXAAEnabled; }

	void SetFXAASubpixelBlend(float InValue);
	void SetFXAAEdgeThreshold(float InValue);
	void SetFXAAEdgeThresholdMin(float InValue);

	float GetFXAASubpixelBlend() const { return FXAAUserParameters.SubpixelBlend; }
	float GetFXAAEdgeThreshold() const { return FXAAUserParameters.EdgeThreshold; }
	float GetFXAAEdgeThresholdMin() const { return FXAAUserParameters.EdgeThresholdMin; }
private:
	UPipeline* Pipeline = nullptr;
	UDeviceResources* DeviceResources = nullptr;
	UFontRenderer* FontRenderer = nullptr;
	TArray<UPrimitiveComponent*> PrimitiveComponents;

	// States
	ID3D11DepthStencilState* DefaultDepthStencilState = nullptr;
	ID3D11DepthStencilState* DecalDepthStencilState = nullptr;
	ID3D11DepthStencilState* DisabledDepthStencilState = nullptr;
	ID3D11BlendState* AlphaBlendState = nullptr;
	ID3D11BlendState* AdditiveBlendState = nullptr;

	// Constant Buffers
	ID3D11Buffer* ConstantBufferModels = nullptr;
	ID3D11Buffer* ConstantBufferViewProj = nullptr;
	ID3D11Buffer* ConstantBufferColor = nullptr;
	ID3D11Buffer* ConstantBufferBatchLine = nullptr;

	FLOAT ClearColor[4] = {0.025f, 0.025f, 0.025f, 1.0f};

	// Default Shaders
	ID3D11VertexShader* DefaultVertexShader = nullptr;
	ID3D11PixelShader* DefaultPixelShader = nullptr;
	ID3D11InputLayout* DefaultInputLayout = nullptr;
	
	// Texture Shaders
	ID3D11VertexShader* TextureVertexShader = nullptr;
	ID3D11PixelShader* TexturePixelShader = nullptr;
	ID3D11InputLayout* TextureInputLayout = nullptr;

	ID3D11VertexShader* DecalVertexShader = nullptr;
	ID3D11PixelShader* DecalPixelShader = nullptr;
	ID3D11InputLayout* DecalInputLayout = nullptr;

	// Depth Shaders (for Scene Depth ViewMode)
	ID3D11VertexShader* DepthVertexShader = nullptr;
	ID3D11PixelShader* DepthPixelShader = nullptr;
	ID3D11InputLayout* DepthInputLayout = nullptr;

	uint32 Stride = 0;

	FViewport* ViewportClient = nullptr;
	
	bool bIsResizing = false;

	bool bIsFXAAEnabled = false;

	ID3D11VertexShader* FXAAVertexShader = nullptr;
	ID3D11PixelShader* FXAAPixelShader = nullptr;
	ID3D11SamplerState* FXAASamplerState = nullptr;

	ID3D11Buffer* ConstantBufferFXAAParameters = nullptr;
	FFXAAParameters FXAAUserParameters;

	void CreatePostProcessResources();
	void ReleasePostProcessResources();
	void ExecuteFXAA();
	void UpdateFXAAConstantBuffer();

	TArray<class FRenderPass*> RenderPasses;
};