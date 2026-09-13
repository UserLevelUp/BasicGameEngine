#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "imgui.h"

// Forward declaration
class OpNode;

// Surface reflection styles (bge.ui.buttons.glass.overlays)
enum class BgeGlassOverlayStyle {
    None,
    CloverFourLeaf,
    WindowFourPane,
    SpecularSheen,
    FrostedDiffuse
};

// Descriptor for an individual glass button (Invariant: legible text under glass)
struct BgeGlassButtonDescriptor {
    std::string id;
    std::string label;          // Primary description text displayed under glass (Invariant 1: high contrast)
    std::string command;        // Canonical BGE command to execute
    std::string icon;           // Icon identifier
    std::string group;          // Group membership
    BgeGlassOverlayStyle overlay = BgeGlassOverlayStyle::CloverFourLeaf;
    float overlayAlpha = 0.25f; // Light overtone (guarded: 0.10f - 0.35f)
    float cornerRadius = 8.0f;
    uint32_t textColor = 0xFFFFFFFF; // High contrast white
    uint32_t glassTint = 0x401A2536; // Translucent frosted body
    bool enabled = true;
};

// Descriptor for a button group (hierarchical rail/panel)
struct BgeGlassButtonGroupDescriptor {
    std::string id;
    std::string label;
    std::string icon;
    BgeGlassOverlayStyle defaultOverlay = BgeGlassOverlayStyle::CloverFourLeaf;
    std::vector<BgeGlassButtonDescriptor> buttons;
};

// Descriptor for a menu item (bge.ui.menu)
struct BgeGlassMenuItemDescriptor {
    std::string id;
    std::string label;
    std::string command;
    std::vector<BgeGlassMenuItemDescriptor> children;
};

// Procedural Overlay Renderers (Top Layer)
void DrawOverlayCloverFourLeaf(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha);
void DrawOverlayWindowFourPane(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha);
void DrawOverlaySpecularSheen(ImDrawList* drawList, ImVec2 min, ImVec2 max, float alpha);

// Core Glass Rendering Functions (3-Layer Model)
bool DrawGlassButton(ImDrawList* drawList, const BgeGlassButtonDescriptor& desc, ImVec2 size,
                     const std::function<void(const std::string&, const std::string&)>& onClick = nullptr);

void DrawGlassButtonGroup(ImDrawList* drawList, const BgeGlassButtonGroupDescriptor& group,
                          const std::function<void(const std::string&, const std::string&)>& onClick = nullptr);

// OpNode Bridge
std::vector<BgeGlassButtonGroupDescriptor> BuildGlassGroupsFromOpNode(const std::shared_ptr<OpNode>& rootNode);
