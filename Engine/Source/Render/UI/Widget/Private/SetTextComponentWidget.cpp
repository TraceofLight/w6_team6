#include "pch.h"
#include "Render/UI/Widget/Public/SetTextComponentWidget.h"

#include "Level/Public/Level.h"
#include "Actor/Public/CubeActor.h"
#include "Actor/Public/SphereActor.h"
#include "Actor/Public/SquareActor.h"
#include "Actor/Public/TriangleActor.h"
#include "Actor/Public/StaticMeshActor.h"
#include "Actor/Public/BillBoardActor.h"
#include "Actor/Public/TextActor.h"
#include "Component/Public/TextComponent.h"

#include <climits>

IMPLEMENT_CLASS(USetTextComponentWidget, UWidget)

USetTextComponentWidget::USetTextComponentWidget()
	: UWidget("Set TextComponent Widget")
{
}

USetTextComponentWidget::~USetTextComponentWidget() = default;

void USetTextComponentWidget::Initialize()
{
	// Do Nothing Here
}

void USetTextComponentWidget::Update()
{
	// 매 프레임 Level의 선택된 Actor를 확인해서 정보 반영
	// TODO(KHJ): 적절한 위치를 찾을 것
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (CurrentLevel)
	{
		AActor* NewSelectedActor = GEditor->GetEditorModule()->GetSelectedActor();

		// Update Current Selected Actor
		if (SelectedActor != NewSelectedActor)
		{
			SelectedActor = NewSelectedActor;
		}

		// Get Current Selected Actor Information
		if (SelectedActor)
			UpdateTextFromActor();
	}
}

void USetTextComponentWidget::RenderWidget()
{
    if (!SelectedActor) return;
    if (!SelectedTextComponent) return;

    ImGui::Separator();
    ImGui::Text("Type Text");
    ImGui::Spacing();

    // 선택 변화 감지하여 buf 갱신
    static UTextComponent* sLastComp = nullptr;
    static char buf[256] = { 0 };

    if (SelectedTextComponent != sLastComp)
    {
        const FString& text = SelectedTextComponent->GetText();
        size_t n = std::min(text.size(), sizeof(buf) - 1);
        memcpy(buf, text.c_str(), n);
        buf[n] = '\0';
        sLastComp = SelectedTextComponent;
    }

    ImGui::PushID(SelectedTextComponent);
    // 텍스트 변경 시마다 즉시 적용 (실시간 반영)
    if (ImGui::InputText("Type Text", buf, IM_ARRAYSIZE(buf)))
    {
        SelectedTextComponent->SetText(FString(buf));
    }
    // 포커스 잃은 뒤 최종 커밋도 원하면:
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        SelectedTextComponent->SetText(FString(buf));
    }
    ImGui::PopID();
}

void USetTextComponentWidget::UpdateTextFromActor()
{
    // 1) 에디터 선택 컴포넌트가 UTextComponent면 우선
    if (auto* sel = GEditor->GetEditorModule()->GetSelectedComponent())
        if (auto* txt = Cast<UTextComponent>(sel)) { SelectedTextComponent = txt; return; }

    // 2) 아니면 액터에서 첫 Text를 선택
    SelectedTextComponent = nullptr;
    for (UActorComponent* Comp : SelectedActor->GetOwnedComponents())
        if (auto* txt = Cast<UTextComponent>(Comp)) { SelectedTextComponent = txt; return; }
}
