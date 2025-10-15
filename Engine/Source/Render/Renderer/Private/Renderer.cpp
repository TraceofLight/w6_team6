#include "pch.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/FontRenderer/Public/FontRenderer.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Component/Public/HeightFogComponent.h"
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
	CreateFullscreenQuad();
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

	// Scene RT는 ViewportClient 초기화 후에 생성 (올바른 크기 사용)
	CreateSceneRenderTargets();

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
	ReleaseSceneRenderTargets();
	ReleaseConstantBuffers();
	ReleaseDefaultShader();
	ReleaseDepthShader();
	ReleaseFullscreenQuad();
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

	// No Test But Write Depth (Depth Test X, Depth Write O) - 포스트 프로세스용
	D3D11_DEPTH_STENCIL_DESC NoTestWriteDescription = {};
	NoTestWriteDescription.DepthEnable = TRUE;  // Depth 활성화 (write를 위해)
	NoTestWriteDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;  // Depth write 활성화
	NoTestWriteDescription.DepthFunc = D3D11_COMPARISON_ALWAYS;  // 항상 통과 (test 비활성화)
	NoTestWriteDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&NoTestWriteDescription, &NoTestButWriteDepthState);
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
	SafeRelease(NoTestButWriteDepthState);
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

		UCamera* CurrentCamera = &ViewportClient.Camera;
		CurrentCamera->Update(ViewportClient.GetViewportInfo());
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, CurrentCamera->GetFViewProjConstants());
		Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

		const D3D11_VIEWPORT& ClientViewport = ViewportClient.GetViewportInfo();

		// === Scene RT 렌더링: clientViewport와 동일한 viewport 사용 ===
		// Scene RT는 SwapChain 전체 크기로 생성되었으므로
		// 각 ViewportClient의 TopLeftX/Y를 그대로 사용하여 해당 영역에 렌더링

		// IMPORTANT: 각 viewport마다 Scene RT를 다시 바인딩
		// (이전 viewport의 post-processing이 BackBuffer로 바인딩을 변경했으므로)
		ID3D11RenderTargetView* SceneRtvs[] = { SceneColorRTV };
		GetDeviceContext()->OMSetRenderTargets(1, SceneRtvs, SceneDepthDSV);
		GetDeviceContext()->RSSetViewports(1, &ClientViewport);

		{
			TIME_PROFILE(RenderLevel)
			RenderLevel(CurrentCamera);
		}

		// === 디버그 프리미티브 렌더링: Scene RT에 렌더링 (FXAA 적용) ===
		{
			TIME_PROFILE(RenderDebugPrimitives)
			GEditor->GetEditorModule()->RenderDebugPrimitives(CurrentCamera);
		}

		// === Post-Processing: Scene RT -> 백버퍼 ===
		// 통합 포스트 프로세싱 패스: Fog + Anti-Aliasing (FXAA)
		// RenderLevel + RenderDebugPrimitives 결과에 모두 FXAA 적용
		GetDeviceContext()->RSSetViewports(1, &ClientViewport);

		{
			TIME_PROFILE(ExecutePostProcess)
			ExecutePostProcess(CurrentCamera, ClientViewport); // Fog + FXAA 통합
		}

		// === 기즈모 렌더링: BackBuffer에 직접 렌더링 (FXAA 미적용, 항상 선명) ===
		{
			TIME_PROFILE(RenderGizmo)
			GEditor->GetEditorModule()->RenderGizmo(CurrentCamera);
		}
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
	// BackBuffer 클리어 (post-processing 결과를 받을 곳)
	auto* BackBufferRTV = DeviceResources->GetRenderTargetView();
	auto* BackBufferDSV = DeviceResources->GetDepthStencilView();
	GetDeviceContext()->ClearRenderTargetView(BackBufferRTV, ClearColor);
	GetDeviceContext()->ClearDepthStencilView(BackBufferDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Scene RT 클리어 및 바인딩 (Scene Color + Scene Depth)
	// 이후 각 ViewportClient가 Scene RT의 해당 영역에 렌더링함
	GetDeviceContext()->ClearRenderTargetView(SceneColorRTV, ClearColor);
	GetDeviceContext()->ClearDepthStencilView(SceneDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	ID3D11RenderTargetView* SceneRtvs[] = { SceneColorRTV };
	GetDeviceContext()->OMSetRenderTargets(1, SceneRtvs, SceneDepthDSV);

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

void URenderer::OnResize(uint32 InWidth, uint32 InHeight)
{
	if (!DeviceResources || !GetDeviceContext() || !GetSwapChain()) return;

	this->ReleaseSceneRenderTargets();
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

	// Recreate Scene Render Targets with new size
	this->CreateSceneRenderTargets();

	auto* RenderTargetView = DeviceResources->GetRenderTargetView();
	ID3D11RenderTargetView* RenderTargetViews[] = { RenderTargetView };
	GetDeviceContext()->OMSetRenderTargets(1, RenderTargetViews, DeviceResources->GetDepthStencilView());
}

void URenderer::CreatePostProcessResources()
{
	// PostProcess 셰이더 로드 (통합 포스트 프로세싱: Fog + FXAA)
	TArray<D3D11_INPUT_ELEMENT_DESC> PostProcessLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/PostProcess.hlsl",
		PostProcessLayout,
		&PostProcessVertexShader,
		&PostProcessInputLayout
	);

	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/PostProcess.hlsl",
		&PostProcessPixelShader
	);

	// 선형 클램프 샘플러
	PostProcessSamplerState = FRenderResourceFactory::CreateSamplerState(
		D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP
	);
	ConstantBufferPostProcessParameters = FRenderResourceFactory::CreateConstantBuffer<FPostProcessParameters>();
	PostProcessUserParameters = {}; // 기본값(위 구조체 디폴트)
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPostProcessParameters, PostProcessUserParameters);
}

void URenderer::ReleasePostProcessResources()
{
	SafeRelease(PostProcessVertexShader);
	SafeRelease(PostProcessInputLayout);
	SafeRelease(PostProcessPixelShader);
	SafeRelease(PostProcessSamplerState);
	SafeRelease(ConstantBufferPostProcessParameters);
}

void URenderer::ExecutePostProcess(UCamera* InCurrentCamera, const D3D11_VIEWPORT& InViewport)
{
	auto* Context = GetDeviceContext();
	const ULevel* CurrentLevel = GWorld->GetLevel();

	// 출력: 백버퍼 RTV로
	auto* BackBufferRTV = DeviceResources->GetRenderTargetView();
	auto* BackBufferDSV = DeviceResources->GetDepthStencilView();
	Context->OMSetRenderTargets(1, &BackBufferRTV, BackBufferDSV);

	// Viewport 설정 (각 ViewportClient 영역에만 적용)
	Context->RSSetViewports(1, &InViewport);

	// PostProcess 상수 버퍼 업데이트 (viewport + fog + FXAA 정보)
	FPostProcessParameters postProcessParams = PostProcessUserParameters; // 기존 사용자 파라미터 복사

	// FXAA 활성화 플래그 설정
	postProcessParams.EnableFXAA = bIsFXAAEnabled ? 1.0f : 0.0f;

	// Viewport 정보 설정
	postProcessParams.ViewportTopLeft = FVector2(InViewport.TopLeftX, InViewport.TopLeftY);
	postProcessParams.ViewportSize = FVector2(InViewport.Width, InViewport.Height);

	// Scene RT 크기 가져오기
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	GetSwapChain()->GetDesc(&swapChainDesc);
	postProcessParams.SceneRTSize = FVector2(
		static_cast<float>(swapChainDesc.BufferDesc.Width),
		static_cast<float>(swapChainDesc.BufferDesc.Height)
	);

	// Fog 파라미터 설정
	const bool bShowFog = CurrentLevel && (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_Fog) != 0;

	// Find first enabled HeightFogComponent
	UHeightFogComponent* FogComponent = nullptr;
	if (CurrentLevel && bShowFog)
	{
		for (AActor* Actor : CurrentLevel->GetActors())
		{
			if (!Actor) continue;
			for (UActorComponent* Comp : Actor->GetOwnedComponents())
			{
				if (auto* Fog = Cast<UHeightFogComponent>(Comp))
				{
					if (Fog->IsEnabled())
					{
						FogComponent = Fog;
						break;
					}
				}
			}
			if (FogComponent) break;
		}
	}

	// Fog 파라미터 채우기
	if (FogComponent && bShowFog)
	{
		postProcessParams.FogDensity = FogComponent->GetFogDensity();
		postProcessParams.FogHeightFalloff = FogComponent->GetFogHeightFalloff();
		postProcessParams.StartDistance = FogComponent->GetStartDistance();
		postProcessParams.FogCutoffDistance = FogComponent->GetFogCutoffDistance();
		postProcessParams.FogMaxOpacity = FogComponent->GetFogMaxOpacity();

		FVector4 ColorRGBA = FogComponent->GetFogInscatteringColor();
		postProcessParams.FogInscatteringColor = FVector(ColorRGBA.X, ColorRGBA.Y, ColorRGBA.Z);

		postProcessParams.CameraPosition = InCurrentCamera->GetLocation();
		postProcessParams.FogHeight = FogComponent->GetWorldLocation().Z;
	}
	else
	{
		// Fog 비활성화
		postProcessParams.FogDensity = 0.0f;
		postProcessParams.FogMaxOpacity = 0.0f;
	}

	// Inverse View-Projection Matrix
	const FViewProjConstants& ViewProj = InCurrentCamera->GetFViewProjConstants();
	FMatrix ViewProjMatrix = ViewProj.View * ViewProj.Projection;
	postProcessParams.InvViewProj = ViewProjMatrix.Inverse();

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPostProcessParameters, postProcessParams);

	// 파이프라인 셋업
	FPipelineInfo PipelineInfo = {
		PostProcessInputLayout,                     // PostProcess fullscreen quad layout
		PostProcessVertexShader,                    // PostProcess VS (fullscreen quad)
		FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid }),
		NoTestButWriteDepthState,                   // Depth test X, Depth write O
		PostProcessPixelShader,                     // PostProcess PS (Fog + FXAA 통합)
		nullptr,                                    // Blend
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetConstantBuffer(0, false, ConstantBufferPostProcessParameters);

	// 소스 텍스처/샘플러 (Scene Color + Scene Depth)
	ID3D11ShaderResourceView* srvs[2] = { SceneColorSRV, SceneDepthSRV };
	Context->PSSetShaderResources(0, 2, srvs);
	Pipeline->SetSamplerState(0, false, PostProcessSamplerState);

	// Fullscreen Quad 그리기 (RenderFog와 동일한 방식)
	uint32 stride = sizeof(float) * 5;  // Position(3) + TexCoord(2)
	uint32 offset = 0;
	Pipeline->SetVertexBuffer(FullscreenQuadVB, stride);
	Pipeline->SetIndexBuffer(FullscreenQuadIB, sizeof(uint32));
	Pipeline->DrawIndexed(6, 0, 0);

	// SRV 언바인드(경고 방지)
	ID3D11ShaderResourceView* NullSrvs[2] = { nullptr, nullptr };
	Context->PSSetShaderResources(0, 2, NullSrvs);
}
void URenderer::UpdatePostProcessConstantBuffer()
{
	if (ConstantBufferPostProcessParameters)
	{
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPostProcessParameters, PostProcessUserParameters);
	}
}

void URenderer::SetFXAASubpixelBlend(float InValue)
{
	float Clamped = std::clamp(InValue, 0.0f, 1.0f);
	if (PostProcessUserParameters.SubpixelBlend != Clamped)
	{
		PostProcessUserParameters.SubpixelBlend = Clamped;
		UpdatePostProcessConstantBuffer();
	}
}

void URenderer::SetFXAAEdgeThreshold(float InValue)
{
	// 일반적으로 0.05 ~ 0.25 권장
	float Clamped = std::clamp(InValue, 0.01f, 0.5f);
	if (PostProcessUserParameters.EdgeThreshold != Clamped)
	{
		PostProcessUserParameters.EdgeThreshold = Clamped;
		UpdatePostProcessConstantBuffer();
	}
}

void URenderer::SetFXAAEdgeThresholdMin(float InValue)
{
	// 일반적으로 0.002 ~ 0.05 권장
	float Clamped = std::clamp(InValue, 0.001f, 0.1f);
	if (PostProcessUserParameters.EdgeThresholdMin != Clamped)
	{
		PostProcessUserParameters.EdgeThresholdMin = Clamped;
		UpdatePostProcessConstantBuffer();
	}
}

void URenderer::CreateFullscreenQuad()
{
	struct
	{
		FVector Position;
		float U, V;
	} vertices[] =
	{
		{ FVector(-1.0f,  1.0f, 0.0f), 0.0f, 0.0f },  // Top-left
		{ FVector( 1.0f,  1.0f, 0.0f), 1.0f, 0.0f },  // Top-right
		{ FVector(-1.0f, -1.0f, 0.0f), 0.0f, 1.0f },  // Bottom-left
		{ FVector( 1.0f, -1.0f, 0.0f), 1.0f, 1.0f }   // Bottom-right
	};

	uint32 indices[] = { 0, 1, 2, 2, 1, 3 };

	D3D11_BUFFER_DESC vbd = {};
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(vertices);
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vInitData = {};
	vInitData.pSysMem = vertices;
	GetDevice()->CreateBuffer(&vbd, &vInitData, &FullscreenQuadVB);

	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(indices);
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA iInitData = {};
	iInitData.pSysMem = indices;
	GetDevice()->CreateBuffer(&ibd, &iInitData, &FullscreenQuadIB);
}

void URenderer::ReleaseFullscreenQuad()
{
	SafeRelease(FullscreenQuadVB);
	SafeRelease(FullscreenQuadIB);
}

void URenderer::CreateSceneRenderTargets()
{
	// Scene RT는 SwapChain 전체 크기로 생성 (4분할 viewport 지원)
	// 각 ViewportClient는 Scene RT의 해당 영역에 렌더링됨
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	GetSwapChain()->GetDesc(&swapChainDesc);
	uint32 Width = swapChainDesc.BufferDesc.Width;
	uint32 Height = swapChainDesc.BufferDesc.Height;

	if (Width == 0 || Height == 0)
	{
		UE_LOG("CreateSceneRenderTargets: Invalid SwapChain size %ux%u", Width, Height);
		return;
	}

	UE_LOG("CreateSceneRenderTargets: Creating %ux%u textures (SwapChain full size)", Width, Height);

	// Scene Color Render Target
	D3D11_TEXTURE2D_DESC ColorDescription = {};
	ColorDescription.Width = Width;
	ColorDescription.Height = Height;
	ColorDescription.MipLevels = 1;
	ColorDescription.ArraySize = 1;
	ColorDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ColorDescription.SampleDesc.Count = 1;
	ColorDescription.SampleDesc.Quality = 0;
	ColorDescription.Usage = D3D11_USAGE_DEFAULT;
	ColorDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	ColorDescription.CPUAccessFlags = 0;
	ColorDescription.MiscFlags = 0;

	GetDevice()->CreateTexture2D(&ColorDescription, nullptr, &SceneColorTexture);

	// RTV 생성 (씬 렌더링용)
	D3D11_RENDER_TARGET_VIEW_DESC RTVDescription = {};
	RTVDescription.Format = ColorDescription.Format;
	RTVDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDescription.Texture2D.MipSlice = 0;
	GetDevice()->CreateRenderTargetView(SceneColorTexture, &RTVDescription, &SceneColorRTV);

	// SRV 생성 (포스트 프로세스에서 읽기용)
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDescription = {};
	SRVDescription.Format = ColorDescription.Format;
	SRVDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDescription.Texture2D.MostDetailedMip = 0;
	SRVDescription.Texture2D.MipLevels = 1;
	GetDevice()->CreateShaderResourceView(SceneColorTexture, &SRVDescription, &SceneColorSRV);

	// Scene Depth Texture (SRV 지원)
	D3D11_TEXTURE2D_DESC DepthDescription = {};
	DepthDescription.Width = Width;
	DepthDescription.Height = Height;
	DepthDescription.MipLevels = 1;
	DepthDescription.ArraySize = 1;
	DepthDescription.Format = DXGI_FORMAT_R24G8_TYPELESS; // Typeless로 생성 (DSV와 SRV 모두 지원)
	DepthDescription.SampleDesc.Count = 1;
	DepthDescription.SampleDesc.Quality = 0;
	DepthDescription.Usage = D3D11_USAGE_DEFAULT;
	DepthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	DepthDescription.CPUAccessFlags = 0;
	DepthDescription.MiscFlags = 0;

	GetDevice()->CreateTexture2D(&DepthDescription, nullptr, &SceneDepthTexture);

	// DSV 생성 (Depth 쓰기용)
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDescription = {};
	DSVDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  // Depth 24비트 + Stencil 8비트
	DSVDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDescription.Texture2D.MipSlice = 0;
	GetDevice()->CreateDepthStencilView(SceneDepthTexture, &DSVDescription, &SceneDepthDSV);

	// SRV 생성 (Depth 읽기용 - Stencil은 무시)
	D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc = {};
	depthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;  // Depth만 읽기, Stencil 무시
	depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	depthSRVDesc.Texture2D.MostDetailedMip = 0;
	depthSRVDesc.Texture2D.MipLevels = 1;
	GetDevice()->CreateShaderResourceView(SceneDepthTexture, &depthSRVDesc, &SceneDepthSRV);

	UE_LOG("Scene Render Targets Created: %ux%u", Width, Height);
}

void URenderer::ReleaseSceneRenderTargets()
{
	SafeRelease(SceneColorSRV);
	SafeRelease(SceneColorRTV);
	SafeRelease(SceneColorTexture);

	SafeRelease(SceneDepthSRV);
	SafeRelease(SceneDepthDSV);
	SafeRelease(SceneDepthTexture);
}

