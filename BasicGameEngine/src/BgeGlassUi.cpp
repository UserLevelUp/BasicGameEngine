#include "../include/BgeGlassUi.h"

#include <cmath>
#include <algorithm>
#include "../../OpNode/OpNode.h"

namespace {
void DrawRoundedGradient(ImDrawList* list, ImVec2 min, ImVec2 max, ImU32 top, ImU32 bottom, float rounding) {
    const int first = list->VtxBuffer.Size;
    list->AddRectFilled(min, max, IM_COL32_WHITE, rounding);
    const auto a = ImGui::ColorConvertU32ToFloat4(top);
    const auto b = ImGui::ColorConvertU32ToFloat4(bottom);
    const float height = (std::max)(1.0f, max.y - min.y);
    for (int i = first; i < list->VtxBuffer.Size; ++i) {
        auto& vertex = list->VtxBuffer[i];
        const float t = (std::clamp)((vertex.pos.y - min.y) / height, 0.0f, 1.0f);
        const float coverage = static_cast<float>((vertex.col >> IM_COL32_A_SHIFT) & 255) / 255.0f;
        vertex.col = ImGui::ColorConvertFloat4ToU32(ImVec4(a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t, (a.w+(b.w-a.w)*t)*coverage));
    }
}
}

void DrawOverlayCloverFourLeaf(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha) {
    if (!drawList || alpha <= 0.0f) return;

    // Subtle 4-leaf clover reflection in the upper-left quadrant of the glass button
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    const float size = (std::min)(width, height) * 0.38f;
    const ImVec2 center(min.x + size * 0.85f, min.y + size * 0.85f);
    const float petalRadius = size * 0.26f;
    const float offset = petalRadius * 0.75f;

    const ImU32 cloverColor = ImColor(1.0f, 1.0f, 1.0f, (std::min)(alpha, 0.35f));
    const ImU32 centerGlint = ImColor(1.0f, 1.0f, 1.0f, (std::min)(alpha * 1.3f, 0.45f));

    // 4 rounded petals
    drawList->AddCircleFilled(ImVec2(center.x, center.y - offset), petalRadius, cloverColor, 16);
    drawList->AddCircleFilled(ImVec2(center.x + offset, center.y), petalRadius, cloverColor, 16);
    drawList->AddCircleFilled(ImVec2(center.x, center.y + offset), petalRadius, cloverColor, 16);
    drawList->AddCircleFilled(ImVec2(center.x - offset, center.y), petalRadius, cloverColor, 16);

    // Center subtle glint
    drawList->AddCircleFilled(center, petalRadius * 0.45f, centerGlint, 12);
}

void DrawOverlayWindowFourPane(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha) {
    if (!drawList || alpha <= 0.0f) return;

    // Subtle 4-pane lit window reflection across the glass surface
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    
    // Window reflection bounds (angled quadrant reflection)
    const float startX = min.x + width * 0.12f;
    const float endX = min.x + width * 0.58f;
    const float startY = min.y + height * 0.10f;
    const float endY = min.y + height * 0.70f;
    const float midX = (startX + endX) * 0.5f;
    const float midY = (startY + endY) * 0.5f;
    const float gap = 2.0f;

    const ImU32 paneColor = ImColor(0.92f, 0.96f, 1.0f, (std::min)(alpha, 0.35f));
    const float rounding = 2.0f;

    // Top-Left pane
    drawList->AddRectFilled(ImVec2(startX, startY), ImVec2(midX - gap, midY - gap), paneColor, rounding);
    // Top-Right pane
    drawList->AddRectFilled(ImVec2(midX + gap, startY), ImVec2(endX, midY - gap), paneColor, rounding);
    // Bottom-Left pane
    drawList->AddRectFilled(ImVec2(startX, midY + gap), ImVec2(midX - gap, endY), paneColor, rounding);
    // Bottom-Right pane
    drawList->AddRectFilled(ImVec2(midX + gap, midY + gap), ImVec2(endX, endY), paneColor, rounding);
}

void DrawOverlaySpecularSheen(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha) {
    if (!drawList || alpha <= 0.0f) return;

    // Crisp dual specular band across top bevel
    const float height = max.y - min.y;
    const ImU32 sheenCol1 = ImColor(1.0f, 1.0f, 1.0f, (std::min)(alpha * 1.2f, 0.35f));
    const ImU32 sheenCol2 = ImColor(1.0f, 1.0f, 1.0f, 0.0f);

    // Diagonal specular sweep (6 arguments: min, max, top-left, top-right, bot-right, bot-left)
    drawList->AddRectFilledMultiColor(
        min,
        ImVec2(max.x, min.y + height * 0.45f),
        sheenCol1, sheenCol1, sheenCol2, sheenCol2
    );
}

bool DrawGlassButton(ImDrawList* drawList, const BgeGlassButtonDescriptor& desc, ImVec2 size,
                     const std::function<void(const std::string&, const std::string&)>& onClick) {
    if (!drawList) return false;

    // Standard public Dear ImGui button interaction
    ImGui::BeginDisabled(!desc.enabled);
    bool pressed = ImGui::InvisibleButton(desc.id.c_str(), size, ImGuiButtonFlags_EnableNav) && desc.enabled;
    ImGui::EndDisabled();
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();

    if (pressed && onClick) {
        onClick(desc.id, desc.command);
    }

    // ==========================================
    // LAYER 1: BASE LAYER (Functional Core)
    // High-contrast text, drop shadows, icon
    // ==========================================
    ImVec4 tint = ImGui::ColorConvertU32ToFloat4(desc.glassTint);
    const float light = held ? -0.02f : (hovered ? 0.04f : 0.0f);
    tint.x = (std::clamp)(tint.x + light, 0.0f, 1.0f);
    tint.y = (std::clamp)(tint.y + light, 0.0f, 1.0f);
    tint.z = (std::clamp)(tint.z + light, 0.0f, 1.0f);
    if (!desc.enabled) tint.w *= 0.55f;
    const ImU32 baseBg = ImGui::ColorConvertFloat4ToU32(tint);
    drawList->AddRectFilled(min, max, baseBg, desc.cornerRadius);

    // Crisp text sizing and centering
    const char* labelText = desc.label.c_str();
    const ImVec2 textSize = ImGui::CalcTextSize(labelText, nullptr, true);
    const ImVec2 textPos(
        min.x + (size.x - textSize.x) * 0.5f,
        min.y + (size.y - textSize.y) * 0.5f + (held ? 1.0f : 0.0f)
    );

    // Drop shadow for guaranteed readability under glass
    drawList->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 180), labelText);
    // Primary crisp label
    drawList->AddText(textPos, desc.textColor, labelText);

    // ==========================================
    // LAYER 2: MIDDLE LAYER (Glass Body)
    // Translucent gradient, refractive border
    // ==========================================
    const ImU32 glassTop = hovered ? IM_COL32(255, 255, 255, 45) : IM_COL32(255, 255, 255, 25);
    const ImU32 glassBot = hovered ? IM_COL32(255, 255, 255, 10) : IM_COL32(255, 255, 255, 4);
    DrawRoundedGradient(drawList, min, max, glassTop, glassBot, desc.cornerRadius);

    // Beveled rim stroke
    const ImU32 borderCol = hovered ? IM_COL32(180, 220, 255, 160)
                                    : IM_COL32(120, 160, 200, 90);
    drawList->AddRect(min, max, borderCol, desc.cornerRadius, 0, 1.2f);

    // Inner top edge highlight
    drawList->AddLine(
        ImVec2(min.x + desc.cornerRadius * 0.5f, min.y + 1.0f),
        ImVec2(max.x - desc.cornerRadius * 0.5f, min.y + 1.0f),
        IM_COL32(255, 255, 255, hovered ? 140 : 80),
        1.0f
    );

    // ==========================================
    // LAYER 3: TOP LAYER (Reflection Overlay)
    // Guarded light overtones (0.10 <= alpha <= 0.35)
    // ==========================================
    const float alpha = (std::clamp)(desc.overlayAlpha, 0.10f, 0.35f);
    const auto paintReflection = [&]() {
    switch (desc.overlay) {
        case BgeGlassOverlayStyle::CloverFourLeaf:
            DrawOverlayCloverFourLeaf(drawList, min, max, alpha);
            break;
        case BgeGlassOverlayStyle::WindowFourPane:
            DrawOverlayWindowFourPane(drawList, min, max, alpha);
            break;
        case BgeGlassOverlayStyle::SpecularSheen:
            DrawOverlaySpecularSheen(drawList, min, max, alpha);
            break;
        case BgeGlassOverlayStyle::FrostedDiffuse:
            DrawRoundedGradient(drawList, min, max, IM_COL32(210, 230, 245, 22), IM_COL32(180, 205, 220, 8), desc.cornerRadius);
            break;
        default:
            break;
    }
    };
    // Keep the functional label and its shadow clear of reflection highlights.
    // Four disjoint clips preserve the reflection geometry outside that area.
    const ImVec2 guardMin((std::max)(min.x, textPos.x - 3.0f), (std::max)(min.y, textPos.y - 2.0f));
    const ImVec2 guardMax((std::min)(max.x, textPos.x + textSize.x + 3.0f), (std::min)(max.y, textPos.y + textSize.y + 2.0f));
    const auto paintClip = [&](ImVec2 a, ImVec2 b) {
        if (a.x >= b.x || a.y >= b.y) return;
        drawList->PushClipRect(a, b, true);
        paintReflection();
        drawList->PopClipRect();
    };
    paintClip(min, ImVec2(max.x, guardMin.y));
    paintClip(ImVec2(min.x, guardMax.y), max);
    paintClip(ImVec2(min.x, guardMin.y), ImVec2(guardMin.x, guardMax.y));
    paintClip(ImVec2(guardMax.x, guardMin.y), ImVec2(max.x, guardMax.y));

    return pressed;
}

void DrawGlassButtonGroup(ImDrawList* drawList, const BgeGlassButtonGroupDescriptor& group,
                          const std::function<void(const std::string&, const std::string&)>& onClick) {
    if (!drawList) return;

    ImGui::TextDisabled("%s", group.label.c_str());
    ImGui::Spacing();

    const float btnWidth = 140.0f;
    const float btnHeight = 36.0f;
    const float spacing = 8.0f;

    for (size_t i = 0; i < group.buttons.size(); ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, spacing);
        }
        DrawGlassButton(drawList, group.buttons[i], ImVec2(btnWidth, btnHeight), onClick);
    }
    ImGui::Spacing();
}

std::vector<BgeGlassButtonGroupDescriptor> BuildGlassGroupsFromOpNode(const std::shared_ptr<OpNode>& rootNode) {
    std::vector<BgeGlassButtonGroupDescriptor> groups;
    if (!rootNode) return groups;

    for (const auto& child : rootNode->GetChildren()) {
        if (!child) continue;
        std::string nodeName = child->GetValue("name");
        if (nodeName.empty()) nodeName = "Group";

        BgeGlassButtonGroupDescriptor group;
        group.id = nodeName;
        group.label = child->GetValue("label");
        if (group.label.empty()) group.label = nodeName;
        group.icon = child->GetValue("icon");

        for (const auto& btnNode : child->GetChildren()) {
            if (!btnNode) continue;
            BgeGlassButtonDescriptor btn;
            btn.id = btnNode->GetValue("name");
            btn.label = btnNode->GetValue("label");
            btn.command = btnNode->GetValue("command");
            btn.icon = btnNode->GetValue("icon");
            btn.group = group.id;

            std::string overlayStr = btnNode->GetValue("overlay");
            if (overlayStr == "clover-four-leaf") {
                btn.overlay = BgeGlassOverlayStyle::CloverFourLeaf;
            } else if (overlayStr == "window-four-pane") {
                btn.overlay = BgeGlassOverlayStyle::WindowFourPane;
            } else if (overlayStr == "specular-sheen") {
                btn.overlay = BgeGlassOverlayStyle::SpecularSheen;
            } else {
                btn.overlay = BgeGlassOverlayStyle::None;
            }

            std::string alphaStr = btnNode->GetValue("overlay_alpha");
            if (!alphaStr.empty()) {
                try {
                    btn.overlayAlpha = std::stof(alphaStr);
                } catch (...) {
                    btn.overlayAlpha = 0.25f;
                }
            }

            group.buttons.push_back(btn);
        }
        groups.push_back(group);
    }
    return groups;
}
