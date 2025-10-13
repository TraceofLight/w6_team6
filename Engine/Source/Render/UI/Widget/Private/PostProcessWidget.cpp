#include "pch.h"
#include "Render/UI/Widget/Public/PostProcessWidget.h"
#include "Render/Renderer/Public/Renderer.h"

void UPostProcessWidget::RenderWidget()
{
    if (ImGui::CollapsingHeader("Post Process - FXAA", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto& RendererInstance = URenderer::GetInstance();

        bool IsFXAAEnabled = RendererInstance.IsFXAAEnabled();
        if (ImGui::Checkbox("Enable FXAA", &IsFXAAEnabled))
        {
            RendererInstance.SetFXAAEnabled(IsFXAAEnabled);
        }

        float SubpixelBlend = RendererInstance.GetFXAASubpixelBlend();
        if (ImGui::DragFloat("Subpixel Blend", &SubpixelBlend, 0.01f, 0.0f, 1.0f, "%.3f"))
        {
            RendererInstance.SetFXAASubpixelBlend(SubpixelBlend);
        }

        float EdgeThreshold = RendererInstance.GetFXAAEdgeThreshold();
        if (ImGui::DragFloat("Edge Threshold", &EdgeThreshold, 0.001f, 0.01f, 0.5f, "%.4f"))
        {
            RendererInstance.SetFXAAEdgeThreshold(EdgeThreshold);
        }

        float EdgeThresholdMin = RendererInstance.GetFXAAEdgeThresholdMin();
        if (ImGui::DragFloat("Edge Threshold Min", &EdgeThresholdMin, 0.001f, 0.001f, 0.1f, "%.4f"))
        {
            RendererInstance.SetFXAAEdgeThresholdMin(EdgeThresholdMin);
        }

        ImGui::Text("흐릿하면 Subpixel Blend를 낮추고,");
        ImGui::Text("Edge Threshold/Min을 올려 에지 검출을 줄여보세요.");
        ImGui::Text("Subpixel Blend : 0.30 ~ 0.55 (낮출수록 선명, 계단 가능성↑)");
        ImGui::Text("Edge Threshold : 0.12 ~ 0.20 (값↑ → 에지 민감도↓ → 흐림↓)");
        ImGui::Text("Edge Threshold Min: 0.02 ~ 0.05 (값↑ → 약한 에지 무시↑ → 흐림↓)");
    }
}