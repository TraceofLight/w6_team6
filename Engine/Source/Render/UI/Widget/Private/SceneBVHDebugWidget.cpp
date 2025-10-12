#include "pch.h"
#include "Render/UI/Widget/Public/SceneBVHDebugWidget.h"
#include "Global/SceneBVH.h"
#include "Level/Public/Level.h"
#include "ImGui/imgui.h"

USceneBVHDebugWidget::USceneBVHDebugWidget()
{
}

void USceneBVHDebugWidget::RenderWidget()
{
	if (ImGui::Begin("Scene BVH Debug"))
	{
		ULevel* Level = GWorld->GetLevel();
		if (!Level)
		{
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "No Level Found");
			ImGui::End();
			return;
		}

		// BVH 토글
		if (ImGui::Checkbox("Show BVH", &bShowBVH))
		{
			Level->ToggleSceneBVHVisualization(bShowBVH, MaxDepth);
		}

		// 깊이 슬라이더
		if (ImGui::SliderInt("Max Depth", &MaxDepth, -1, 10))
		{
			if (bShowBVH)
			{
				Level->ToggleSceneBVHVisualization(true, MaxDepth);
			}
		}

		ImGui::Separator();

		// BVH 빌드 버튼
		if (ImGui::Button("Build BVH"))
		{
			Level->BuildSceneBVH();
			if (bShowBVH)
			{
				Level->ToggleSceneBVHVisualization(true, MaxDepth);
			}
		}

		ImGui::SameLine();

		// 시각화 업데이트 버튼
		if (ImGui::Button("Update Visualization"))
		{
			Level->ToggleSceneBVHVisualization(bShowBVH, MaxDepth);
		}

		ImGui::Separator();

		// BVH 정보 표시
		FSceneBVH* BVH = Level->GetSceneBVH();
		if (BVH)
		{
			ImGui::Text("BVH Info:");
			ImGui::Text("  Nodes: %d", BVH->GetNodeCount());
			ImGui::Text("  Root Index: %d", BVH->GetRootIndex());

			const TArray<FAABB>& Boxes = Level->GetCachedDebugBoxes();
			const TArray<FVector4>& Colors = Level->GetCachedDebugColors();
			ImGui::Text("  Visible Boxes: %d", static_cast<int>(Boxes.size()));
			ImGui::Text("  Visible Colors: %d", static_cast<int>(Colors.size()));
		}
		else
		{
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "BVH Not Built");
			ImGui::Text("Click 'Build BVH' to create");
		}

		ImGui::Separator();

		// 색상 범례
		if (ImGui::TreeNode("Color Legend"))
		{
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "Red:     Depth 0 (Root)");
			ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Orange:  Depth 1");
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Yellow:  Depth 2");
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "Green:   Depth 3");
			ImGui::TextColored(ImVec4(0, 1, 1, 1), "Cyan:    Depth 4");
			ImGui::TextColored(ImVec4(0, 0, 1, 1), "Blue:    Depth 5");
			ImGui::TextColored(ImVec4(0.5f, 0, 1, 1), "Purple:  Depth 6");
			ImGui::TextColored(ImVec4(1, 0, 1, 1), "Magenta: Depth 7+");
			ImGui::TreePop();
		}

		ImGui::End();
	}
}
