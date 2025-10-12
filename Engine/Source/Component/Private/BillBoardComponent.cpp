#include "pch.h"
#include "Component/Public/BillBoardComponent.h""
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Physics/Public/AABB.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"
#include "Texture/Public/Texture.h"
#include "Texture/Public/TextureRenderProxy.h"

IMPLEMENT_CLASS(UBillBoardComponent, UPrimitiveComponent)

UBillBoardComponent::UBillBoardComponent()
{
	Type = EPrimitiveType::Sprite;
	
    UAssetManager& ResourceManager = UAssetManager::GetInstance();

	Vertices = ResourceManager.GetVertexData(Type);
	VertexBuffer = ResourceManager.GetVertexbuffer(Type);
	NumVertices = ResourceManager.GetNumVertices(Type);

	Indices = ResourceManager.GetIndexData(Type);
	IndexBuffer = ResourceManager.GetIndexbuffer(Type);
	NumIndices = ResourceManager.GetNumIndices(Type);

	RenderState.CullMode = ECullMode::None;
	RenderState.FillMode = EFillMode::Solid;
	BoundingBox = &ResourceManager.GetAABB(Type);

    Sampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
    if (!Sampler)
    {
        assert(false);
    }

    const TMap<FName, ID3D11ShaderResourceView*>& TextureCache = \
        UAssetManager::GetInstance().GetTextureCache();
    if (!TextureCache.empty())
        Sprite = *TextureCache.begin();
}

UBillBoardComponent::~UBillBoardComponent()
{
    if (Sampler)
        Sampler->Release();
}

void UBillBoardComponent::FaceCamera(
    const FVector& CameraPosition,
    const FVector& CameraUp,
    const FVector& FallbackUp
)
{
    // Front 
    FVector Front = (CameraPosition - GetRelativeLocation());
    Front.Normalize();

    // Right 
    FVector Right = CameraUp.Cross(Front);
    if (Right.Length() <= 0.0001f)
    {
        // CameraUp Front FallbackUp 
        Right = FallbackUp.Cross(Front);
    }
    Right.Normalize();

    // Up 
    FVector Up = Front.Cross(Right);
    Up.Normalize();

    float XAngle = atan2(Up.Y, Up.Z);
    float YAngle = -asin(Up.X);
    float ZAngle = -atan2(-Right.X, Front.X);

    // 
    SetRelativeRotation(
        FVector(
            FVector::GetRadianToDegree(XAngle),
            FVector::GetRadianToDegree(YAngle),
            FVector::GetRadianToDegree(ZAngle)
        )
    );
}

const TPair<FName, ID3D11ShaderResourceView*>& UBillBoardComponent::GetSprite() const
{
    return Sprite;
}

void UBillBoardComponent::SetSprite(const TPair<FName, ID3D11ShaderResourceView*>& InSprite)
{
    Sprite = InSprite;
}

void UBillBoardComponent::SetSprite(const UTexture* InSprite)
{
    FName SpriteName = InSprite->GetName();
    Sprite = { SpriteName, InSprite->GetRenderProxy()->GetSRV() };
}

ID3D11SamplerState* UBillBoardComponent::GetSampler() const
{ 
    return Sampler;
};

UClass* UBillBoardComponent::GetSpecificWidgetClass() const
{
    return USpriteSelectionWidget::StaticClass();
}

const FRenderState& UBillBoardComponent::GetClassDefaultRenderState()
{
    static FRenderState DefaultRenderState { ECullMode::None, EFillMode::Solid };
    return DefaultRenderState;
}

void UBillBoardComponent::UpdateBillboardMatrix(const FVector& InCameraLocation)
{
    // 1) 부모 트랜스폼 반영된 '자기' 월드 위치
    const FVector basePos = GetWorldLocation();

    // 2) 먼저 최종 표시 위치 계산(위로 띄우기). 화면상 위가 자연스럽게 보이도록 '빌보드 Up' 기준 또는 월드 Up 기준 선택
    const FVector worldUp(0, 0, 1);
    FVector forward = InCameraLocation - basePos;
    if (forward.LengthSquared() < 1e-8f) forward = FVector(0, 0, 1);
    forward.Normalize();

    FVector right = worldUp.Cross(forward);
    if (right.LengthSquared() < 1e-6f) // 정수직 특이점 보정
    {
        const FVector altUp(1, 0, 0);
        right = altUp.Cross(forward);
    }
    right.Normalize();

    FVector up = forward.Cross(right);
    up.Normalize();

    // 오프셋 반영 위치 P (빌보드 Up 축으로 띄우기)
    const FVector P = basePos + up * ZOffset;

    // 3) 회전 후 번역(엔진 규약: R * T)
    const FMatrix R = FMatrix(forward, right, up);
    const FMatrix T = FMatrix::TranslationMatrix(P);
    RTMatrix = R * T;
}

UObject* UBillBoardComponent::Duplicate()
{
    UBillBoardComponent* BillBoardComponent = Cast<UBillBoardComponent>(Super::Duplicate());

    // 고유 필드 복사
    BillBoardComponent->Sprite = Sprite;
    BillBoardComponent->ZOffset = ZOffset;
    BillBoardComponent->RTMatrix = RTMatrix;

    // COM 리소스 공유 시 AddRef 필요(둘 다 Release 호출하므로)
    BillBoardComponent->Sampler = Sampler;
    if (BillBoardComponent->Sampler) 
    { 
        BillBoardComponent->Sampler->AddRef(); 
    }

    return BillBoardComponent;
}