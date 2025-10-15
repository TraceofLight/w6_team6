#include "pch.h"
#include "Component/Public/OrbitComponent.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/UI/Widget/Public/OrbitComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"
#include "Physics/Public/AABB.h"

IMPLEMENT_CLASS(UOrbitComponent, UPrimitiveComponent)

UOrbitComponent::UOrbitComponent()
{
	ComponentType = EComponentType::Primitive;
	Type = EPrimitiveType::Line;
	Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	RenderState.CullMode = ECullMode::None;
	RenderState.FillMode = EFillMode::WireFrame;
	bCanEverTick = false;

	// 기본 궤도 생성
	RegenerateOrbit();
	CreateOrbitBuffers();

	// 바운딩 박스 설정
	BoundingBox = new FAABB(FVector(-Radius, -Radius, -Radius), FVector(Radius, Radius, Radius));
}

UOrbitComponent::~UOrbitComponent()
{
	ReleaseOrbitBuffers();
}

void UOrbitComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UOrbitComponent::TickComponent()
{
	Super::TickComponent();
}

void UOrbitComponent::SetRadius(float InRadius)
{
	if (InRadius <= 0.0f)
	{
		InRadius = 1.0f;
	}

	Radius = InRadius;
	RegenerateOrbit();
	CreateOrbitBuffers();

	// 바운딩 박스 업데이트
	if (BoundingBox)
	{
		delete BoundingBox;
	}
	BoundingBox = new FAABB(FVector(-Radius, -Radius, -Radius), FVector(Radius, Radius, Radius));
	MarkAsDirty();
}

void UOrbitComponent::SetSegments(int32 InSegments)
{
	if (InSegments < 3)
	{
		InSegments = 3;
	}
	if (InSegments > 360)
	{
		InSegments = 360;
	}

	Segments = InSegments;
	RegenerateOrbit();
	CreateOrbitBuffers();
}

void UOrbitComponent::SetOrbitColor(const FVector4& InColor)
{
	SetColor(InColor);
}

void UOrbitComponent::RegenerateOrbit()
{
	OrbitVertices.clear();
	OrbitIndices.clear();

	// XY 평면에 원형 궤도 생성
	const float AngleStep = 360.0f / static_cast<float>(Segments);
	const float RadiansStep = (PI * 2.0f) / static_cast<float>(Segments);

	// 버텍스 생성
	for (int32 i = 0; i < Segments; ++i)
	{
		float Angle = RadiansStep * static_cast<float>(i);
		float X = Radius * cosf(Angle);
		float Y = Radius * sinf(Angle);
		float Z = 0.0f;

		FNormalVertex Vertex;
		Vertex.Position = FVector(X, Y, Z);
		Vertex.Normal = FVector(0.0f, 0.0f, 1.0f);
		OrbitVertices.push_back(Vertex);
	}

	// 인덱스 생성 (라인 세그먼트)
	for (int32 i = 0; i < Segments; ++i)
	{
		OrbitIndices.push_back(i);
		OrbitIndices.push_back((i + 1) % Segments);
	}

	// 포인터 업데이트
	Vertices = &OrbitVertices;
	Indices = &OrbitIndices;
	NumVertices = static_cast<uint32>(OrbitVertices.size());
	NumIndices = static_cast<uint32>(OrbitIndices.size());
}

void UOrbitComponent::CreateOrbitBuffers()
{
	// 기존 버퍼 해제
	ReleaseOrbitBuffers();

	if (OrbitVertices.empty() || OrbitIndices.empty())
	{
		return;
	}

	ID3D11Device* Device = URenderer::GetInstance().GetDevice();
	if (!Device)
	{
		return;
	}

	// 버텍스 버퍼 생성
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VBDesc.ByteWidth = static_cast<uint32>(OrbitVertices.size() * sizeof(FNormalVertex));
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VBDesc.CPUAccessFlags = 0;
	VBDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA VInitData = {};
	VInitData.pSysMem = OrbitVertices.data();

	Device->CreateBuffer(&VBDesc, &VInitData, &DynamicVertexBuffer);

	// 인덱스 버퍼 생성
	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IBDesc.ByteWidth = static_cast<uint32>(OrbitIndices.size() * sizeof(uint32));
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IBDesc.CPUAccessFlags = 0;
	IBDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA IInitData = {};
	IInitData.pSysMem = OrbitIndices.data();

	Device->CreateBuffer(&IBDesc, &IInitData, &DynamicIndexBuffer);

	// 버퍼 포인터 업데이트
	VertexBuffer = DynamicVertexBuffer;
	IndexBuffer = DynamicIndexBuffer;
}

void UOrbitComponent::ReleaseOrbitBuffers()
{
	if (DynamicVertexBuffer)
	{
		DynamicVertexBuffer->Release();
		DynamicVertexBuffer = nullptr;
		VertexBuffer = nullptr;
	}

	if (DynamicIndexBuffer)
	{
		DynamicIndexBuffer->Release();
		DynamicIndexBuffer = nullptr;
		IndexBuffer = nullptr;
	}
}

UObject* UOrbitComponent::Duplicate()
{
	UOrbitComponent* NewComponent = Cast<UOrbitComponent>(Super::Duplicate());

	NewComponent->Radius = Radius;
	NewComponent->Segments = Segments;

	// 새 컴포넌트에 대해 버텍스와 버퍼 재생성
	NewComponent->RegenerateOrbit();
	NewComponent->CreateOrbitBuffers();

	// 바운딩 박스 복제
	if (BoundingBox)
	{
		NewComponent->BoundingBox = new FAABB(FVector(-Radius, -Radius, -Radius), FVector(Radius, Radius, Radius));
	}

	return NewComponent;
}

void UOrbitComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

UClass* UOrbitComponent::GetSpecificWidgetClass() const
{
	return UOrbitComponentWidget::StaticClass();
}

void UOrbitComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "Radius", Radius, 30.0f, false);
		FJsonSerializer::ReadInt32(InOutHandle, "Segments", Segments, 360, false);

		// 로드 후 궤도 재생성
		RegenerateOrbit();
		CreateOrbitBuffers();

		// 바운딩 박스 업데이트
		if (BoundingBox)
		{
			delete BoundingBox;
		}
		BoundingBox = new FAABB(FVector(-Radius, -Radius, -Radius), FVector(Radius, Radius, Radius));
		MarkAsDirty();
	}
	else
	{
		InOutHandle["Radius"] = Radius;
		InOutHandle["Segments"] = Segments;
	}
}
