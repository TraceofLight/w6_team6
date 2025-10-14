#include "pch.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/FontRenderer/Public/FontRenderer.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Component/Public/SemiLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Viewport.h"
#include "Editor/Public/ViewportClient.h"
#include "Editor/Public/Camera.h"
#include "Level/Public/Level.h"
#include "Manager/UI/Public/UIManager.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Render/RenderPass/Public/RenderPass.h"
#include "Component/Mesh/Public/StaticMesh.h"
#include "Optimization/Public/OcclusionCuller.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/RenderPass/Public/BillboardPass.h"
#include "Render/RenderPass/Public/PrimitivePass.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Render/RenderPass/Public/TextPass.h"
#include "Render/RenderPass/Public/DecalPass.h"

IMPLEMENT_SINGLETON_CLASS_BASE(URenderer)

URenderer::URenderer() = default;

URenderer::~URenderer() = default;

void URenderer::Init(HWND InWindowHandle)
{
	DeviceResources = new UDeviceResources(InWindowHandle);
	Pipeline = new UPipeline(GetDeviceContext());
	ViewportClient = new FViewport();

	// 렌더링 상태 및 리소스 생성
	CreateDepthStencilState();
	CreateBlendState();
	CreateDefaultShader();
	CreateTextureShader();
	CreateDecalShader();
	CreateDepthShader();
	CreateConstantBuffers();
	CreatePostProcessResources();

	// FontRenderer 초기화
	FontRenderer = new UFontRenderer();
	if (!FontRenderer->Initialize())
	{
		UE_LOG("FontRenderer 초기화 실패");
		SafeDelete(FontRenderer);
	}

	ViewportClient->InitializeLayout(DeviceResources->GetViewportInfo());

	FStaticMeshPass* StaticMeshPass = new FStaticMeshPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TextureInputLayout, DefaultDepthStencilState,
		DepthVertexShader, DepthPixelShader, DepthInputLayout);
	RenderPasses.push_back(StaticMeshPass);

	FPrimitivePass* PrimitivePass = new FPrimitivePass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		DefaultVertexShader, DefaultPixelShader, DefaultInputLayout, DefaultDepthStencilState,
		DepthVertexShader, DepthPixelShader, DepthInputLayout);
	RenderPasses.push_back(PrimitivePass);

	// 알파 블렌딩을 사용하는 일반 데칼 패스
	FDecalPass* AlphaDecalPass = new FDecalPass(Pipeline, ConstantBufferViewProj,
		DecalVertexShader, DecalPixelShader, DecalInputLayout, DecalDepthStencilState, AlphaBlendState, false);
	RenderPasses.push_back(AlphaDecalPass);

	// 가산 혼합을 사용하는 SemiLight 데칼 패스
	FDecalPass* AdditiveDecalPass = new FDecalPass(Pipeline, ConstantBufferViewProj,
		DecalVertexShader, DecalPixelShader, DecalInputLayout, DecalDepthStencilState, AdditiveBlendState, true);
	RenderPasses.push_back(AdditiveDecalPass);

	FBillboardPass* BillboardPass = new FBillboardPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TextureInputLayout, DefaultDepthStencilState);
	RenderPasses.push_back(BillboardPass);

	FTextPass* TextPass = new FTextPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels);
	RenderPasses.push_back(TextPass);
}

void URenderer::Release()
{
	ReleaseConstantBuffers();
	ReleaseDefaultShader();
	ReleaseDepthShader();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	ReleasePostProcessResources();
	FRenderResourceFactory::ReleaseRasterizerState();
	for (auto& RenderPass : RenderPasses)
	{
		RenderPass->Release();
		SafeDelete(RenderPass);
	}

	SafeDelete(ViewportClient);
	SafeDelete(FontRenderer);
	SafeDelete(Pipeline);
	SafeDelete(DeviceResources);
}

void URenderer::CreateDepthStencilState()
{
	// 3D Default Depth Stencil (Depth O, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DefaultDescription = {};
	DefaultDescription.DepthEnable = TRUE;
	DefaultDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DefaultDescription.DepthFunc = D3D11_COMPARISON_LESS;
	DefaultDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DefaultDescription, &DefaultDepthStencilState);

	// Decal Depth Stencil (Depth Read, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DecalDescription = {};
	DecalDescription.DepthEnable = TRUE;
	DecalDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DecalDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DecalDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DecalDescription, &DecalDepthStencilState);

	// Disabled Depth Stencil (Depth X, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DisabledDescription = {};
	DisabledDescription.DepthEnable = FALSE;
	DisabledDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DisabledDescription, &DisabledDepthStencilState);
}

void URenderer::CreateBlendState()
{
    // Alpha Blending
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&blendDesc, &AlphaBlendState);

    // Additive Blending (for lights)
    D3D11_BLEND_DESC additiveDesc = {};
    additiveDesc.RenderTarget[0].BlendEnable = TRUE;
    additiveDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    additiveDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    additiveDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    additiveDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    additiveDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    additiveDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    additiveDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&additiveDesc, &AdditiveBlendState);
}

void URenderer::CreateDefaultShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DefaultLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/SampleShader.hlsl", DefaultLayout, &DefaultVertexShader, &DefaultInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/SampleShader.hlsl", &DefaultPixelShader);
	Stride = sizeof(FNormalVertex);
}

void URenderer::CreateDecalShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DecalLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/DecalShader.hlsl", DecalLayout, &DecalVertexShader, &DecalInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/DecalShader.hlsl", &DecalPixelShader);
}

void URenderer::CreateTextureShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/TextureShader.hlsl", TextureLayout, &TextureVertexShader, &TextureInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TextureShader.hlsl", &TexturePixelShader);
}

void URenderer::CreateDepthShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DepthLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/DepthShader.hlsl", DepthLayout, &DepthVertexShader, &DepthInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/DepthShader.hlsl", &DepthPixelShader);
}

void URenderer::CreateConstantBuffers()
{
	ConstantBufferModels = FRenderResourceFactory::CreateConstantBuffer<FMatrix>();
	ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
	ConstantBufferViewProj = FRenderResourceFactory::CreateConstantBuffer<FViewProjConstants>();
}

void URenderer::ReleaseConstantBuffers()
{
	SafeRelease(ConstantBufferModels);
	SafeRelease(ConstantBufferColor);
	SafeRelease(ConstantBufferViewProj);
}

void URenderer::ReleaseDefaultShader()
{
	SafeRelease(DefaultInputLayout);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultVertexShader);
	SafeRelease(TextureInputLayout);
	SafeRelease(TexturePixelShader);
	SafeRelease(TextureVertexShader);
	SafeRelease(DecalVertexShader);
	SafeRelease(DecalPixelShader);
}

void URenderer::ReleaseDepthShader()
{
	SafeRelease(DepthInputLayout);
	SafeRelease(DepthPixelShader);
	SafeRelease(DepthVertexShader);
}

void URenderer::ReleaseDepthStencilState()
{
	SafeRelease(DefaultDepthStencilState);
	SafeRelease(DecalDepthStencilState);
	SafeRelease(DisabledDepthStencilState);
	if (GetDeviceContext())
	{
		GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
	}
}

void URenderer::ReleaseBlendState()
{
    SafeRelease(AlphaBlendState);
    SafeRelease(AdditiveBlendState);
}

void URenderer::Update()
{
	RenderBegin();

	for (FViewportClient& ViewportClient : ViewportClient->GetViewports())
	{
		if (ViewportClient.GetViewportInfo().Width < 1.0f || ViewportClient.GetViewportInfo().Height < 1.0f) { continue; }

		ViewportClient.Apply(GetDeviceContext());

		UCamera* CurrentCamera = &ViewportClient.Camera;
		CurrentCamera->Update(ViewportClient.GetViewportInfo());
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, CurrentCamera->GetFViewProjConstants());
		Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

		{
			TIME_PROFILE(RenderLevel)
			RenderLevel(CurrentCamera);
		}
		{
			TIME_PROFILE(RenderEditor)
			GEditor->GetEditorModule()->RenderEditor(CurrentCamera);
		}
	}
	if (bIsFXAAEnabled)
	{
		ExecuteFXAA();
	}
	{
		TIME_PROFILE(UUIManager)
		UUIManager::GetInstance().Render();
	}
	{
		TIME_PROFILE(UStatOverlay)
		UStatOverlay::GetInstance().Render();
	}

	RenderEnd();
}

void URenderer::RenderBegin() const
{
	auto* RenderTargetView = DeviceResources->GetRenderTargetView();
	auto* DepthStencilView = DeviceResources->GetDepthStencilView();

	if (bIsFXAAEnabled)
	{
		auto* SceneRTV = DeviceResources->GetSceneColorRTV();
		GetDeviceContext()->ClearRenderTargetView(SceneRTV, ClearColor);
		GetDeviceContext()->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		ID3D11RenderTargetView* Rtvs[] = { SceneRTV };
		GetDeviceContext()->OMSetRenderTargets(1, Rtvs, DepthStencilView);
	}
	else
	{
		GetDeviceContext()->ClearRenderTargetView(RenderTargetView, ClearColor);
		GetDeviceContext()->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

		ID3D11RenderTargetView* Rtvs[] = { RenderTargetView };
		GetDeviceContext()->OMSetRenderTargets(1, Rtvs, DepthStencilView);
	}
	DeviceResources->UpdateViewport();
}

void URenderer::RenderLevel(UCamera* InCurrentCamera)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) { return; }
	
	const FViewProjConstants& ViewProj = InCurrentCamera->GetFViewProjConstants();
	TArray<UPrimitiveComponent*> FinalVisiblePrims = InCurrentCamera->GetViewVolumeCuller().GetRenderableObjects();

	// // 오클루전 컬링 수행
	// TIME_PROFILE(Occlusion)
	// static COcclusionCuller Culler;
	// Culler.InitializeCuller(ViewProj.View, ViewProj.Projection);
	// FinalVisiblePrims = Culler.PerformCulling(
	// 	InCurrentCamera->GetViewVolumeCuller().GetRenderableObjects(),
	// 	InCurrentCamera->GetLocation()
	// );
	// TIME_PROFILE_END(Occlusion)

	FRenderingContext RenderingContext(&ViewProj, InCurrentCamera, GEditor->GetEditorModule()->GetViewMode(), CurrentLevel->GetShowFlags());
	RenderingContext.AllPrimitives = FinalVisiblePrims;
	for (auto& Prim : FinalVisiblePrims)
	{
		if (auto StaticMesh = Cast<UStaticMeshComponent>(Prim))
		{
			RenderingContext.StaticMeshes.push_back(StaticMesh);
		}
		else if (auto BillBoard = Cast<UBillBoardComponent>(Prim))
		{
			RenderingContext.BillBoards.push_back(BillBoard);
		}
		else if (auto Text = Cast<UTextComponent>(Prim); Text && !Text->IsExactly(UUUIDTextComponent::StaticClass()))
		{
			RenderingContext.Texts.push_back(Text);
		}
		else if (!Prim->IsA(UUUIDTextComponent::StaticClass()))
		{
			RenderingContext.DefaultPrimitives.push_back(Prim);
		}
	}
	// 수집 전에 플래그 확인
	const bool bWantsDecal = (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_Decal) != 0;
	if (bWantsDecal)
	{
		UStatOverlay::GetInstance().ResetDecalFrame();
		for (auto Decal : CurrentLevel->GetVisibleDecals())
		{
			if (Cast<USemiLightComponent>(Decal->GetParentAttachment()))
			{
				RenderingContext.AdditiveDecals.push_back(Decal);
			}
			else
			{
				RenderingContext.AlphaDecals.push_back(Decal);
			}
		}
		
		UStatOverlay::GetInstance().RecordDecalCollection(
			static_cast<uint32>(CurrentLevel->GetAllDecals().size()),
			static_cast<uint32>(CurrentLevel->GetVisibleDecals().size())
			);
	}

	for (auto RenderPass: RenderPasses)
	{
		RenderPass->Execute(RenderingContext);
	}
}

void URenderer::RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride, uint32 InIndexBufferStride)
{
    // Use the global stride if InStride is 0
    const uint32 FinalStride = (InStride == 0) ? Stride : InStride;

    // Allow for custom shaders, fallback to default
    FPipelineInfo PipelineInfo = {
        InPrimitive.InputLayout ? InPrimitive.InputLayout : DefaultInputLayout,
        InPrimitive.VertexShader ? InPrimitive.VertexShader : DefaultVertexShader,
		FRenderResourceFactory::GetRasterizerState(InRenderState),
        InPrimitive.bShouldAlwaysVisible ? DisabledDepthStencilState : DefaultDepthStencilState,
        InPrimitive.PixelShader ? InPrimitive.PixelShader : DefaultPixelShader,
        nullptr,
        InPrimitive.Topology
    };
    Pipeline->UpdatePipeline(PipelineInfo);

    // Update constant buffers
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels, FMatrix::GetModelMatrix(InPrimitive.Location, FVector::GetDegreeToRadian(InPrimitive.Rotation), InPrimitive.Scale));
	Pipeline->SetConstantBuffer(0, true, ConstantBufferModels);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
	
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, InPrimitive.Color);
	Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);
	Pipeline->SetConstantBuffer(2, true, ConstantBufferColor);
	
    Pipeline->SetVertexBuffer(InPrimitive.Vertexbuffer, FinalStride);

    // The core logic: check for an index buffer
    if (InPrimitive.IndexBuffer && InPrimitive.NumIndices > 0)
    {
        Pipeline->SetIndexBuffer(InPrimitive.IndexBuffer, InIndexBufferStride);
        Pipeline->DrawIndexed(InPrimitive.NumIndices, 0, 0);
    }
    else
    {
        Pipeline->Draw(InPrimitive.NumVertices, 0);
    }
}

void URenderer::RenderEnd() const
{
	TIME_PROFILE(DrawCall)
	GetSwapChain()->Present(0, 0);
	TIME_PROFILE_END(DrawCall)
}

void URenderer::OnResize(uint32 InWidth, uint32 InHeight) const
{
	if (!DeviceResources || !GetDeviceContext() || !GetSwapChain()) return;

	DeviceResources->ReleaseFrameBuffer();
	DeviceResources->ReleaseDepthBuffer();
	GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);

	if (FAILED(GetSwapChain()->ResizeBuffers(2, InWidth, InHeight, DXGI_FORMAT_UNKNOWN, 0)))
	{
		UE_LOG("OnResize Failed");
		return;
	}

	DeviceResources->UpdateViewport();
	DeviceResources->CreateFrameBuffer();
	DeviceResources->CreateDepthBuffer();

	auto* RenderTargetView = DeviceResources->GetRenderTargetView();
	ID3D11RenderTargetView* RenderTargetViews[] = { RenderTargetView };
	GetDeviceContext()->OMSetRenderTargets(1, RenderTargetViews, DeviceResources->GetDepthStencilView());
}

void URenderer::CreatePostProcessResources()
{
	// FXAA 셰이더 로드 (엔트리: mainVS/mainPS)
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/FXAA.hlsl",
		{}, // SV_VertexID 사용으로 InputLayout 없음
		&FXAAVertexShader,
		nullptr
	);
	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/FXAA.hlsl",
		&FXAAPixelShader
	);

	// 선형 클램프 샘플러
	FXAASamplerState = FRenderResourceFactory::CreateSamplerState(
		D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP
	);
	ConstantBufferFXAAParameters = FRenderResourceFactory::CreateConstantBuffer<FFXAAParameters>();
	FXAAUserParameters = {}; // 기본값(위 구조체 디폴트)
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferFXAAParameters, FXAAUserParameters);
}

void URenderer::ReleasePostProcessResources()
{
	SafeRelease(FXAAVertexShader);
	SafeRelease(FXAAPixelShader);
	SafeRelease(FXAASamplerState);
	SafeRelease(ConstantBufferFXAAParameters);
}

void URenderer::ExecuteFXAA()
{
	auto* Context = GetDeviceContext();

	// 출력: 백버퍼 RTV로
	ID3D11RenderTargetView* Rtvs[] = { GetRenderTargetView() };
	Context->OMSetRenderTargets(1, Rtvs, nullptr);
	// 전체(메뉴바 제외) 화면 뷰포트로 복구
	const D3D11_VIEWPORT& FullViewport = DeviceResources->GetViewportInfo();
	Context->RSSetViewports(1, &FullViewport);

	// 파이프라인 셋업
	FPipelineInfo PipelineInfo = {
		nullptr,                                    // InputLayout
		FXAAVertexShader,                           // VS
		FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid }),
		DisabledDepthStencilState,                  // 깊이 비활성
		FXAAPixelShader,                            // PS
		nullptr,                                    // Blend
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	// 소스 텍스처/샘플러
	Pipeline->SetTexture(0, false, DeviceResources->GetSceneColorSRV());
	Pipeline->SetSamplerState(0, false, FXAASamplerState);
	Pipeline->SetConstantBuffer(0, false, ConstantBufferFXAAParameters);
	// 풀스크린 삼각형 드로우
	Pipeline->Draw(3, 0);

	// SRV 언바인드(경고 방지)
	ID3D11ShaderResourceView* NullSrv[1] = { nullptr };
	Context->PSSetShaderResources(0, 1, NullSrv);
}
void URenderer::UpdateFXAAConstantBuffer()
{
	if (ConstantBufferFXAAParameters)
	{
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferFXAAParameters, FXAAUserParameters);
	}
}

void URenderer::SetFXAASubpixelBlend(float InValue)
{
	float Clamped = std::clamp(InValue, 0.0f, 1.0f);
	if (FXAAUserParameters.SubpixelBlend != Clamped)
	{
		FXAAUserParameters.SubpixelBlend = Clamped;
		UpdateFXAAConstantBuffer();
	}
}

void URenderer::SetFXAAEdgeThreshold(float InValue)
{
	// 일반적으로 0.05 ~ 0.25 권장
	float Clamped = std::clamp(InValue, 0.01f, 0.5f);
	if (FXAAUserParameters.EdgeThreshold != Clamped)
	{
		FXAAUserParameters.EdgeThreshold = Clamped;
		UpdateFXAAConstantBuffer();
	}
}

void URenderer::SetFXAAEdgeThresholdMin(float InValue)
{
	// 일반적으로 0.002 ~ 0.05 권장
	float Clamped = std::clamp(InValue, 0.001f, 0.1f);
	if (FXAAUserParameters.EdgeThresholdMin != Clamped)
	{
		FXAAUserParameters.EdgeThresholdMin = Clamped;
		UpdateFXAAConstantBuffer();
	}
}