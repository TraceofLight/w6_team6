#include "pch.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"

#include "Level/Public/Level.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Component/Public/BillBoardComponent.h"

#include <climits>

IMPLEMENT_CLASS(USpriteSelectionWidget, UWidget)

USpriteSelectionWidget::USpriteSelectionWidget()
	: UWidget("Sprite Selection Widget")
{
}

USpriteSelectionWidget::~USpriteSelectionWidget() = default;

void USpriteSelectionWidget::Initialize()
{
	// Do Nothing Here
}

void USpriteSelectionWidget::Update()
{
	// 현재 Level의 선택된 Actor를 확인하여 위젯 상태를 업데이트
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (CurrentLevel)
	{
		AActor* NewSelectedActor = GEditor->GetEditorModule()->GetSelectedActor();

		// Update Current Selected Actor
		if (SelectedActor != NewSelectedActor)
		{
			SelectedActor = NewSelectedActor;

			for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
			{
				if (UBillBoardComponent* UUIDTextComponent = Cast<UBillBoardComponent>(Component))
					{ SelectedBillBoard = UUIDTextComponent; }
			}
		}
	}
}

void USpriteSelectionWidget::RenderWidget()
{
	if (!SelectedActor || !SelectedBillBoard)
		return;
	
	ImGui::Separator();
	ImGui::Text("Select Sprite");

	ImGui::Spacing();
		
	static int CurrentIndex = 0;

	// 1) TextureCache에서 (이름, 표시용 문자열) 수집
	struct FSpriteItem
	{
		FName   Name;
		FString Label;
	};
	TArray<FSpriteItem> SpriteItems;
	SpriteItems.reserve(UAssetManager::GetInstance().GetTextureCache().size());

	const TMap<FName, ID3D11ShaderResourceView*>& TextureCache =
		UAssetManager::GetInstance().GetTextureCache();

	for (auto It = TextureCache.begin(); It != TextureCache.end(); ++It)
	{
		const FName Name = It->first;
		SpriteItems.push_back({ Name, Name.ToString() });
	}

	// 비어 있으면 안전 탈출
	if (SpriteItems.empty())
	{
		ImGui::TextUnformatted("No sprite found.");
		ImGui::Separator();
		WidgetNum = (WidgetNum + 1) % std::numeric_limits<uint32>::max();
		return;
	}

	// 2) 표시 문자열 기준 정렬
	std::sort(SpriteItems.begin(), SpriteItems.end(),
		[](const FSpriteItem& A, const FSpriteItem& B)
		{
			return A.Label < B.Label;
		});

	// 3) 정렬 이후 '현재 선택된 스프라이트'의 인덱스를 다시 찾아 반영
	{
		const FName SelectedName = SelectedBillBoard->GetSprite().first; // FName 가정
		int Found = 0;
		for (int i = 0; i < static_cast<int>(SpriteItems.size()); ++i)
		{
			if (SpriteItems[i].Name == SelectedName)
			{
				Found = i;
				break;
			}
		}
		CurrentIndex = Found;
	}

	// 4) 드롭다운 렌더
	const FString& CurrentLabel = SpriteItems[CurrentIndex].Label;
	if (ImGui::BeginCombo("Sprite", CurrentLabel.c_str()))
	{
		for (int i = 0; i < static_cast<int>(SpriteItems.size()); ++i)
		{
			const bool bIsSelected = (CurrentIndex == i);
			if (ImGui::Selectable(SpriteItems[i].Label.c_str(), bIsSelected))
			{
				CurrentIndex = i;
				SetSpriteOfActor(SpriteItems[CurrentIndex].Name); // ← FName으로 적용
			}
			if (bIsSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();
	WidgetNum = (WidgetNum + 1) % std::numeric_limits<uint32>::max();
}

void USpriteSelectionWidget::SetSpriteOfActor(FName NewSprite)
{
	if (!SelectedActor || !SelectedBillBoard)
	{
		return;
	}

	const TMap<FName, ID3D11ShaderResourceView*>& TextureCache =
		UAssetManager::GetInstance().GetTextureCache();

	auto It = TextureCache.find(NewSprite);
	if (It != TextureCache.end())
	{
		SelectedBillBoard->SetSprite(*It); // (FName, SRV*)의 pair* 라고 가정한 기존 코드 스타일 준수
	}
}
