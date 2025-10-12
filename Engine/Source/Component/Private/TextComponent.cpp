#include "pch.h"
#include "Component/Public/TextComponent.h"

#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/UI/Widget/Public/SetTextComponentWidget.h"
#include "Level/Public/Level.h"

IMPLEMENT_CLASS(UTextComponent, UPrimitiveComponent)

/**
 * @brief 레벨 내 액터들의 UUID를 화면에 표시하는 텍스트(빌보드) 컴포넌트 클래스
 * Actor has a UBillBoardComponent
 */
UTextComponent::UTextComponent()
{
	Type = EPrimitiveType::Text;

	UAssetManager& ResourceManager = UAssetManager::GetInstance();

	Vertices = &PickingAreaVertex;
	NumVertices = PickingAreaVertex.size();

	Indices = &PickingAreaIndex;
	NumIndices = PickingAreaIndex.size();

	RegulatePickingAreaByTextLength();
}

UTextComponent::~UTextComponent()
{
}

FMatrix UTextComponent::GetBoundingTransform() const
{
	// 기본 Text는 월드 변환 사용 (빌보드 특성이 없는 경우)
	return GetWorldTransformMatrix();
}

void UTextComponent::UpdateRotationMatrix(const FVector& InCameraLocation) {}
FMatrix UTextComponent::GetRTMatrix() const { return FMatrix(); }

const FString& UTextComponent::GetText() { return Text; }
void UTextComponent::SetText(const FString& InText)
{
	if (Text == InText) return;           // 불필요한 갱신 방지(선택)

	Text = InText;
	RegulatePickingAreaByTextLength();    // 길이에 맞춘 로컬 박스 재계산

	// 1) AABB 캐시 무효화 + 자식 변환 더티 전파
	MarkAsDirty();

	// 2) 옥트리/가시성 시스템에 바운딩 업데이트 알림
	GWorld->GetLevel()->UpdatePrimitiveInOctree(this);
}

UClass* UTextComponent::GetSpecificWidgetClass() const
{
	return USetTextComponentWidget::StaticClass();
}

UObject* UTextComponent::Duplicate()
{
	UTextComponent* TextComponent = Cast<UTextComponent>(Super::Duplicate());
	// 1) 고유 데이터 복사
	TextComponent->Text = Text;

	// 2) 로컬 저장소 복사
	TextComponent->PickingAreaBoundingBox = PickingAreaBoundingBox; // 값 복사
	TextComponent->PickingAreaVertex = PickingAreaVertex;      // 값 복사

	// 3) 상위 포인터 재연결(원본 메모리를 가리키지 않게)
	TextComponent->BoundingBox = &TextComponent->PickingAreaBoundingBox;
	TextComponent->Vertices = &TextComponent->PickingAreaVertex;
	TextComponent->NumVertices = static_cast<uint32>(TextComponent->PickingAreaVertex.size());
	TextComponent->Indices = &PickingAreaIndex;
	TextComponent->NumIndices = static_cast<uint32>(PickingAreaIndex.size());

	// 4) 캐시/옥트리 동기화
	TextComponent->MarkAsDirty();
	return TextComponent;
}

void UTextComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}

void UTextComponent::RegulatePickingAreaByTextLength()
{
	PickingAreaVertex.clear();
	int32 NewStrLen = Text.size();

	const static TPair<int32, int32> Offset[] =
	{
		{-1, 1},
		{1, 1},
		{-1, -1},
		{1, -1}
	};

	for (int i = 0; i < 4; i++)
	{
		FNormalVertex NewVertex = {
			{0.0f, 0.5f * NewStrLen * Offset[i].first, 0.5f * Offset[i].second},
			{}, {}, {}
		};
		PickingAreaVertex.push_back(NewVertex);
	}

	PickingAreaBoundingBox =
		FAABB(
			FVector(0.0f, -0.5f * NewStrLen, -0.5f),
			FVector(0.0f, 0.5f * NewStrLen, 0.5f)
		);
	BoundingBox = &PickingAreaBoundingBox;
}
