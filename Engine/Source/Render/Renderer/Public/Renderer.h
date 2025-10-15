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

struct FPostProcessParameters
{
	float SubpixelBlend = 0.5f; // 0~1 권장: 0.5
	float EdgeThreshold = 0.125f; // 0.0312~0.25
	float EdgeThresholdMin = 0.0312f; // 0~0.0833
	float EnableFXAA = 1.0f; // FXAA 활성화 플래그 (0.0 = OFF, 1.0 = ON)

	FVector2 ViewportTopLeft;
	FVector2 ViewportSize;
	FVector2 SceneRTSize;
	FVector2 Padding2;

	// Fog Parameters
	float FogDensity;
	float FogHeightFalloff;
	float StartDistance;
	float FogCutoffDistance;

	float FogMaxOpacity;
	FVector FogInscatteringColor;

	FVector CameraPosition;
	float FogHeight;

	FMatrix InvViewProj;
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
	void CreateFireBallShader();
	void CreateFullscreenQuad();
	void CreateConstantBuffers();
	void CreateSceneRenderTargets();

	// Release
	void ReleaseConstantBuffers();
	void ReleaseDefaultShader();
	void ReleaseDepthStencilState();
	void ReleaseBlendState();
	void ReleaseDepthShader();
	void ReleaseFireBallShader();
	void ReleaseFullscreenQuad();
	void ReleaseSceneRenderTargets();

	// Render
	void Update();
	void RenderBegin() const;
	void RenderLevel(UCamera* InCurrentCamera);
	void RenderEnd() const;
	void RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride = 0, uint32 InIndexBufferStride = 0);

	void OnResize(uint32 Inwidth = 0, uint32 InHeight = 0);

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
	ID3D11BlendState* GetFireBallBlendState() const { return FireBallBlendState; }
	ID3D11Buffer* GetConstantBufferModels() const { return ConstantBufferModels; }
	ID3D11Buffer* GetConstantBufferViewProj() const { return ConstantBufferViewProj; }
	ID3D11RenderTargetView* GetSceneColorRTV() const{ return SceneColorRTV; }
	ID3D11ShaderResourceView* GetSceneDepthSRV() const { return SceneDepthSRV; }
	ID3D11DepthStencilView* GetSceneDepthDSV() const { return SceneDepthDSV; }
	ID3D11Texture2D* GetSceneDepthTexture() const { return SceneDepthTexture; }
	ID3D11DepthStencilView* GetReadOnlyDSV() const { return SceneDepthDSV_ReadOnly; }
	ID3D11ShaderResourceView* GetSceneColorSRV() const { return SceneColorSRV; }

	ID3D11VertexShader* GetDepthVertexShader() const { return DepthVertexShader; }
	ID3D11PixelShader* GetDepthPixelShader() const { return DepthPixelShader; }
	ID3D11InputLayout* GetDepthInputLayout() const { return DepthInputLayout; }

	void SetIsResizing(bool isResizing) { bIsResizing = isResizing; }

	void SetFXAAEnabled(bool bEnabled) { bIsFXAAEnabled = bEnabled; }
	bool IsFXAAEnabled() const { return bIsFXAAEnabled; }

	void SetFXAASubpixelBlend(float InValue);
	void SetFXAAEdgeThreshold(float InValue);
	void SetFXAAEdgeThresholdMin(float InValue);

	float GetFXAASubpixelBlend() const { return PostProcessUserParameters.SubpixelBlend; }
	float GetFXAAEdgeThreshold() const { return PostProcessUserParameters.EdgeThreshold; }
	float GetFXAAEdgeThresholdMin() const { return PostProcessUserParameters.EdgeThresholdMin; }
private:
	UPipeline* Pipeline = nullptr;
	UDeviceResources* DeviceResources = nullptr;
	UFontRenderer* FontRenderer = nullptr;
	TArray<UPrimitiveComponent*> PrimitiveComponents;

	// States
	ID3D11DepthStencilState* DefaultDepthStencilState = nullptr;
	ID3D11DepthStencilState* DecalDepthStencilState = nullptr;
	ID3D11DepthStencilState* DisabledDepthStencilState = nullptr;
	ID3D11DepthStencilState* NoTestButWriteDepthState = nullptr;  // Depth test 비활성화, depth write 활성화
	ID3D11BlendState* AlphaBlendState = nullptr;
	ID3D11BlendState* AdditiveBlendState = nullptr;
	ID3D11BlendState* FireBallBlendState = nullptr;

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

	// PostProcess Shaders
	ID3D11VertexShader* PostProcessVertexShader = nullptr;
	ID3D11InputLayout* PostProcessInputLayout = nullptr;

	ID3D11VertexShader* FireBallVertexShader = nullptr;
	ID3D11PixelShader* FireBallPixelShader = nullptr;
	ID3D11InputLayout* FireBallInputLayout = nullptr;
	ID3D11Buffer* CBPerObject = nullptr;
	ID3D11Buffer* CBFireBall = nullptr;

	// Fullscreen Quad for Post-Processing
	ID3D11Buffer* FullscreenQuadVB = nullptr;
	ID3D11Buffer* FullscreenQuadIB = nullptr;

	// Scene Render Targets for Post-Processing
	ID3D11Texture2D* SceneColorTexture = nullptr;
	ID3D11RenderTargetView* SceneColorRTV = nullptr;
	ID3D11ShaderResourceView* SceneColorSRV = nullptr;


	ID3D11Texture2D* SceneDepthTexture = nullptr;
	ID3D11DepthStencilView* SceneDepthDSV = nullptr;
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;
	ID3D11DepthStencilView* SceneDepthDSV_ReadOnly = nullptr;

	uint32 Stride = 0;

	FViewport* ViewportClient = nullptr;

	bool bIsResizing = false;

	bool bIsFXAAEnabled = false;

	ID3D11PixelShader* PostProcessPixelShader = nullptr;
	ID3D11SamplerState* PostProcessSamplerState = nullptr;

	ID3D11Buffer* ConstantBufferPostProcessParameters = nullptr;
	FPostProcessParameters PostProcessUserParameters;

	void CreatePostProcessResources();
	void ReleasePostProcessResources();
	void ExecutePostProcess(UCamera* InCurrentCamera, const D3D11_VIEWPORT& InViewport);
	void UpdatePostProcessConstantBuffer();

	TArray<class FRenderPass*> RenderPasses;
};
