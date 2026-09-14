#include "../include/BgeGlassUi.h"
#include "../include/BgeGlassReflection.h"

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

namespace {
void DrawCurvedReflection(ImDrawList* list, ImVec2 min, ImVec2 max, float alpha,
                          BgeGlassOverlayStyle style, float radius = 8.0f) {
    if (!list || alpha <= 0 || max.x <= min.x || max.y <= min.y) return;
    const float width = max.x-min.x, height = max.y-min.y;
    const int columns = (std::clamp)(static_cast<int>(width/2), 24, 96);
    const int rows = (std::clamp)(static_cast<int>(height/2), 16, 48);
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    // Shared vertices keep the mesh below the 16-bit index limit for normal groups.
    list->PrimReserve(columns*rows*6, (columns+1)*(rows+1));
    const unsigned int base = list->_VtxCurrentIdx;
    for (int row=0; row<rows; ++row) for (int col=0; col<columns; ++col) {
        const unsigned int a = base+row*(columns+1)+col, b=a+1, c=a+columns+1, d=c+1;
        list->PrimWriteIdx(static_cast<ImDrawIdx>(a)); list->PrimWriteIdx(static_cast<ImDrawIdx>(b)); list->PrimWriteIdx(static_cast<ImDrawIdx>(d));
        list->PrimWriteIdx(static_cast<ImDrawIdx>(a)); list->PrimWriteIdx(static_cast<ImDrawIdx>(d)); list->PrimWriteIdx(static_cast<ImDrawIdx>(c));
    }
    for (int row=0; row<=rows; ++row) for (int col=0; col<=columns; ++col) {
        const float u=static_cast<float>(col)/columns, v=static_cast<float>(row)/rows;
        const float opacity = alpha * BgeGlassReflection::Intensity(style,u,v)
            * BgeGlassReflection::Coverage(u*width,v*height,width,height,radius);
        list->PrimWriteVtx(ImVec2(min.x+u*width,min.y+v*height), uv,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.93f,0.97f,1.0f,opacity)));
    }
}
}

void DrawOverlayCloverFourLeaf(ImDrawList* list, ImVec2 min, ImVec2 max, float alpha) {
    DrawCurvedReflection(list,min,max,alpha,BgeGlassOverlayStyle::CloverFourLeaf);
}
void DrawOverlayWindowFourPane(ImDrawList* list, ImVec2 min, ImVec2 max, float alpha) {
    DrawCurvedReflection(list,min,max,alpha,BgeGlassOverlayStyle::WindowFourPane);
}
void DrawOverlaySpecularSheen(ImDrawList* list, ImVec2 min, ImVec2 max, float alpha) {
    DrawCurvedReflection(list,min,max,alpha,BgeGlassOverlayStyle::SpecularSheen);
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
    const int reflectionStart = drawList->VtxBuffer.Size;
    DrawCurvedReflection(drawList, min, max, alpha * (desc.enabled ? 1.0f : 0.45f), desc.overlay, desc.cornerRadius);
    // Soft attenuation over the label keeps the continuous outer surface visible
    // without the old rectangular cutout in the reflection.
    for (int i=reflectionStart; i<drawList->VtxBuffer.Size; ++i) {
        auto& vertex = drawList->VtxBuffer[i];
        const float dx = (std::max)((std::max)(textPos.x-vertex.pos.x, vertex.pos.x-textPos.x-textSize.x),0.0f);
        const float dy = (std::max)((std::max)(textPos.y-vertex.pos.y, vertex.pos.y-textPos.y-textSize.y),0.0f);
        const float attenuation = 0.16f + 0.84f*BgeGlassReflection::Smooth(0,6,std::hypot(dx,dy));
        const auto color = ImGui::ColorConvertU32ToFloat4(vertex.col);
        vertex.col = ImGui::ColorConvertFloat4ToU32(ImVec4(color.x,color.y,color.z,color.w*attenuation));
    }

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
