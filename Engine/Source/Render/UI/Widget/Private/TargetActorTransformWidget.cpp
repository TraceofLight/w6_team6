#include "pch.h"
#include "Render/UI/Widget/Public/TargetActorTransformWidget.h"

#include "Level/Public/Level.h"


UTargetActorTransformWidget::UTargetActorTransformWidget()
	: UWidget("Target Actor Tranform Widget")
{
}

UTargetActorTransformWidget::~UTargetActorTransformWidget() = default;

void UTargetActorTransformWidget::Initialize()
{
	// Do Nothing Here
}

void UTargetActorTransformWidget::Update()
{
	// 매 프레임 Level의 선택된 Actor를 확인해서 정보 반영
	// TODO(KHJ): 적절한 위치를 찾을 것
	ULevel* CurrentLevel = GWorld->GetLevel();

	LevelMemoryByte = CurrentLevel->GetAllocatedBytes();
	LevelObjectCount = CurrentLevel->GetAllocatedCount();

	if (CurrentLevel)
	{
		AActor* CurrentSelectedActor = GEditor->GetEditorModule()->GetSelectedActor();

		// Update Current Selected Actor
		if (SelectedActor != CurrentSelectedActor)
		{
			SelectedActor = CurrentSelectedActor;
		}

		// Get Current Selected Actor Information
		if (SelectedActor)
		{
			UpdateTransformFromActor();
		}
		else if (CurrentSelectedActor)
		{
			SelectedActor = nullptr;
		}
	}
}

void UTargetActorTransformWidget::RenderWidget()
{
	// Memory Information
	// ImGui::Text("레벨 메모리 정보");
	// ImGui::Text("Level Object Count: %u", LevelObjectCount);
	// ImGui::Text("Level Memory: %.3f KB", static_cast<float>(LevelMemoryByte) / KILO);
	// ImGui::Separator();
	AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
	if (SelectedActor)
	{
		ImGui::Separator();
		ImGui::Text("Actor Transform");

		ImGui::Spacing();

		bPositionChanged |= ImGui::DragFloat3("Location", &EditLocation.X, 0.1f);
		// 드래그가 끝났는지 확인
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			bNeedsBVHUpdate = true;
		}

		bRotationChanged |= ImGui::DragFloat3("Rotation", &EditRotation.X, 0.1f);
		// 드래그가 끝났는지 확인
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			bNeedsBVHUpdate = true;
		}

		// Uniform Scale 옵션
		bool bUniformScale = SelectedActor->IsUniformScale();
		if (bUniformScale)
		{
			float UniformScale = EditScale.X;

			if (ImGui::DragFloat("Scale", &UniformScale, 0.01f, 0.01f, 10.0f))
			{
				EditScale = FVector(UniformScale, UniformScale, UniformScale);
				bScaleChanged = true;
			}
			// 드래그가 끝났는지 확인
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				bNeedsBVHUpdate = true;
			}
		}
		else
		{
			bScaleChanged |= ImGui::DragFloat3("Scale", &EditScale.X, 0.1f);
			// 드래그가 끝났는지 확인
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				bNeedsBVHUpdate = true;
			}
		}

		ImGui::Checkbox("Uniform Scale", &bUniformScale);

		SelectedActor->SetUniformScale(bUniformScale);
	}

	ImGui::Separator();
}

/**
 * @brief Render에서 체크된 내용으로 인해 이후 변경되어야 할 내용이 있다면 Change 처리
 */
void UTargetActorTransformWidget::PostProcess()
{
	if (bPositionChanged || bRotationChanged || bScaleChanged)
	{
		ApplyTransformToActor();
		bPositionChanged = bRotationChanged = bScaleChanged = false;
	}

	// 드래그가 끝났을 때만 BVH 업데이트 (자식 컴포넌트 포함)
	if (bNeedsBVHUpdate)
	{
		ULevel* Level = GWorld->GetLevel();
		if (Level)
		{
			Level->UpdateSceneBVHComponent(SelectedActor->GetRootComponent());
		}
		bNeedsBVHUpdate = false;
	}
}

void UTargetActorTransformWidget::UpdateTransformFromActor()
{
	if (SelectedActor)
	{
		EditLocation = SelectedActor->GetActorLocation();
		EditRotation = SelectedActor->GetActorRotation();
		EditScale = SelectedActor->GetActorScale3D();
	}
}

void UTargetActorTransformWidget::ApplyTransformToActor() const
{
	if (SelectedActor)
	{
		SelectedActor->SetActorLocation(EditLocation);
		SelectedActor->SetActorRotation(EditRotation);
		SelectedActor->SetActorScale3D(EditScale);
	}
}
