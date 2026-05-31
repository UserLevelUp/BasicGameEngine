#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

constexpr int BGE_OBJECT_SLOT_COUNT = 10;
constexpr int BGE_PLAYER_ICON_VISIBILITY_MODE_COUNT = 10;

enum class BgeEdgePolicy : int {
    Bounce = 0,
    Wrap = 1,
    Clamp = 2,
};

enum class BgeObjectShape : int {
    Ball = 0,
    Asteroid = 1,
    VectorShip = 2,
    Line = 3,
};

// Render style is recipe-driven, NOT a player/controller mode toggle.
// The composition recipe chooses Filled (lit/shaded) or Outline (single
// stroke colour, classic-Asteroids look) per piece. The renderer reads
// `slot.renderStyle` and dispatches; both controller and standalone
// player exe show whatever the recipe specified, satisfying the
// "reproducible from the recipe" composability bar.
enum class BgeObjectRenderStyle : int {
    Filled = 0,
    Outline = 1,
};

// Heading (facing) is decoupled from velocity so vector-ship pieces can rotate
// in place while inertia drifts independently. Defaults to "pointing right" so
// any object that does not opt in keeps the legacy velocity-derived facing
// (renderers fall back to velocity when both heading components are ~0).

enum class BgeObjectKind : int {
    Generic = 0,
    Player = 1,
    Asteroid = 2,
    Bullet = 3,
};

constexpr int BGE_ASTEROID_POINT_COUNT = 18;

inline constexpr std::array<float, BGE_ASTEROID_POINT_COUNT> BGE_ASTEROID_RADIUS_PROFILE = {
    1.00f, 0.72f, 0.92f, 0.64f, 1.08f, 0.78f,
    0.96f, 0.70f, 1.14f, 0.86f, 0.98f, 0.68f,
    1.06f, 0.74f, 0.90f, 0.62f, 1.10f, 0.82f,
};

inline std::atomic<int>& BgeEdgePolicyStorage()
{
    static std::atomic<int> policy{ static_cast<int>(BgeEdgePolicy::Bounce) };
    return policy;
}

inline std::atomic<int>& BgeRenderTopInsetTenthsStorage()
{
    static std::atomic<int> insetTenths{ 1400 };
    return insetTenths;
}

inline BgeEdgePolicy BgeCurrentEdgePolicy()
{
    return static_cast<BgeEdgePolicy>(BgeEdgePolicyStorage().load(std::memory_order_relaxed));
}

inline void BgeSetCurrentEdgePolicy(BgeEdgePolicy policy)
{
    BgeEdgePolicyStorage().store(static_cast<int>(policy), std::memory_order_relaxed);
}

inline float BgeRenderTopInset()
{
    return static_cast<float>(BgeRenderTopInsetTenthsStorage().load(std::memory_order_relaxed)) / 10.0f;
}

inline void BgeSetRenderTopInset(float inset)
{
    int tenths = static_cast<int>((std::max)(0.0f, inset) * 10.0f + 0.5f);
    BgeRenderTopInsetTenthsStorage().store(tenths, std::memory_order_relaxed);
}

inline const wchar_t* BgeEdgePolicyName(BgeEdgePolicy policy)
{
    switch (policy) {
    case BgeEdgePolicy::Wrap:   return L"wrap";
    case BgeEdgePolicy::Clamp:  return L"clamp";
    case BgeEdgePolicy::Bounce:
    default:                    return L"bounce";
    }
}

inline bool BgeTryParseEdgePolicy(const std::wstring& text, BgeEdgePolicy& out)
{
    if (text == L"bounce") { out = BgeEdgePolicy::Bounce; return true; }
    if (text == L"wrap")   { out = BgeEdgePolicy::Wrap;   return true; }
    if (text == L"clamp")  { out = BgeEdgePolicy::Clamp;  return true; }
    return false;
}

inline const wchar_t* BgeObjectShapeName(BgeObjectShape shape)
{
    switch (shape) {
    case BgeObjectShape::Line: return L"line";
    case BgeObjectShape::VectorShip: return L"vector-ship";
    case BgeObjectShape::Asteroid: return L"asteroid";
    case BgeObjectShape::Ball:
    default:                       return L"ball";
    }
}

inline bool BgeTryParseObjectShape(const std::wstring& text, BgeObjectShape& out)
{
    if (text == L"ball" || text == L"circle") { out = BgeObjectShape::Ball; return true; }
    if (text == L"asteroid" || text == L"rock") { out = BgeObjectShape::Asteroid; return true; }
    if (text == L"vector-ship" || text == L"ship" || text == L"player-ship") { out = BgeObjectShape::VectorShip; return true; }
    if (text == L"line" || text == L"projectile" || text == L"shot") { out = BgeObjectShape::Line; return true; }
    return false;
}

inline const wchar_t* BgeObjectRenderStyleName(BgeObjectRenderStyle style)
{
    switch (style) {
    case BgeObjectRenderStyle::Outline: return L"outline";
    case BgeObjectRenderStyle::Filled:
    default:                            return L"filled";
    }
}

inline bool BgeTryParseObjectRenderStyle(const std::wstring& text, BgeObjectRenderStyle& out)
{
    if (text == L"filled" || text == L"fill" || text == L"solid") { out = BgeObjectRenderStyle::Filled;  return true; }
    if (text == L"outline" || text == L"wireframe" || text == L"wire" || text == L"stroke") {
        out = BgeObjectRenderStyle::Outline; return true;
    }
    return false;
}

inline const wchar_t* BgeObjectKindName(BgeObjectKind kind)
{
    switch (kind) {
    case BgeObjectKind::Player:   return L"player";
    case BgeObjectKind::Asteroid: return L"asteroid";
    case BgeObjectKind::Bullet:   return L"bullet";
    case BgeObjectKind::Generic:
    default:                      return L"generic";
    }
}

struct BgeObjectExtent {
    float minX;
    float maxX;
    float minY;
    float maxY;
};

template <typename Slot>
inline void BgeApplyEdgePolicy(Slot& slot, const BgeObjectExtent& extent, BgeEdgePolicy policy)
{
    switch (policy) {
    case BgeEdgePolicy::Wrap: {
        float spanX = extent.maxX - extent.minX;
        float spanY = extent.maxY - extent.minY;
        if (spanX > 0.0f) {
            if (slot.x < extent.minX) {
                slot.x = extent.maxX - std::fmod(extent.minX - slot.x, spanX);
            }
            else if (slot.x > extent.maxX) {
                slot.x = extent.minX + std::fmod(slot.x - extent.maxX, spanX);
            }
        }
        if (spanY > 0.0f) {
            if (slot.y < extent.minY) {
                slot.y = extent.maxY - std::fmod(extent.minY - slot.y, spanY);
            }
            else if (slot.y > extent.maxY) {
                slot.y = extent.minY + std::fmod(slot.y - extent.maxY, spanY);
            }
        }
        break;
    }
    case BgeEdgePolicy::Clamp: {
        if (slot.x < extent.minX) slot.x = extent.minX;
        else if (slot.x > extent.maxX) slot.x = extent.maxX;
        if (slot.y < extent.minY) slot.y = extent.minY;
        else if (slot.y > extent.maxY) slot.y = extent.maxY;
        break;
    }
    case BgeEdgePolicy::Bounce:
    default: {
        if (slot.x < extent.minX) {
            slot.x = extent.minX;
            slot.velocityX = std::abs(slot.velocityX);
        }
        else if (slot.x > extent.maxX) {
            slot.x = extent.maxX;
            slot.velocityX = -std::abs(slot.velocityX);
        }
        if (slot.y < extent.minY) {
            slot.y = extent.minY;
            slot.velocityY = std::abs(slot.velocityY);
        }
        else if (slot.y > extent.maxY) {
            slot.y = extent.maxY;
            slot.velocityY = -std::abs(slot.velocityY);
        }
        break;
    }
    }
}

struct BgeColorVertex {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
};

struct BgeSceneOverlayText {
    std::wstring text;
    float x = 16.0f;
    float y = 150.0f;
    float scale = 3.0f;
    float r = 0.82f;
    float g = 0.92f;
    float b = 1.0f;
    float a = 1.0f;
    bool centered = false;
};

struct BgeObjectSlotState {
    bool visible = false;
    bool deleteMarked = false;
    bool isDeleted = false;
    bool collisionDetected = false;
    float x = 180.0f;
    float y = 180.0f;
    float velocityX = 180.0f;
    float velocityY = 135.0f;
    float radius = 32.0f;
    float colorR = 0.96f;
    float colorG = 0.34f;
    float colorB = 0.22f;
    float colorA = 1.0f;
    float headingX = 0.0f;
    float headingY = 0.0f;
    BgeObjectShape shape = BgeObjectShape::Ball;
    BgeObjectKind kind = BgeObjectKind::Generic;
    // Recipe-driven render style. Engine itself stays neutral; whatever
    // plugin spawns the slot is responsible for setting this (default
    // Filled keeps legacy behaviour for ball/bouncing-demo recipes).
    BgeObjectRenderStyle renderStyle = BgeObjectRenderStyle::Filled;
    float outlineThickness = 2.0f;  // pixels; used when renderStyle == Outline
};

struct BgePlayerIconVisibilityMode {
    const wchar_t* name;
    BgeObjectShape shape;
    BgeObjectRenderStyle renderStyle;
    float radius;
    float outlineThickness;
    float colorR;
    float colorG;
    float colorB;
    float alpha;
    float invulnerableAlphaHigh;
    float invulnerableAlphaLow;
};

inline const std::array<BgePlayerIconVisibilityMode, BGE_PLAYER_ICON_VISIBILITY_MODE_COUNT>& BgePlayerIconVisibilityModes()
{
    static const std::array<BgePlayerIconVisibilityMode, BGE_PLAYER_ICON_VISIBILITY_MODE_COUNT> modes{{
        { L"baseline outline", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Outline, 14.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 0.54f, 0.30f },
        { L"larger outline", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Outline, 18.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 0.90f, 0.75f },
        { L"big outline", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Outline, 22.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 0.95f, 0.80f },
        { L"thick outline", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Outline, 18.0f, 4.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 0.85f },
        { L"max outline", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Outline, 20.0f, 6.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 0.90f },
        { L"filled white", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 18.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 0.90f },
        { L"cyan outline", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Outline, 18.0f, 4.0f, 0.10f, 0.92f, 1.00f, 1.0f, 1.00f, 0.88f },
        { L"yellow outline", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Outline, 18.0f, 4.0f, 1.00f, 0.92f, 0.16f, 1.0f, 1.00f, 0.88f },
        { L"huge cyan outline", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Outline, 26.0f, 6.0f, 0.52f, 1.00f, 0.94f, 1.0f, 1.00f, 0.92f },
        { L"debug magenta marker", BgeObjectShape::Ball, BgeObjectRenderStyle::Filled, 26.0f, 2.0f, 1.00f, 0.12f, 0.92f, 1.0f, 1.00f, 0.92f },
    }};
    return modes;
}

inline int BgeNormalizePlayerIconVisibilityModeIndex(int modeIndex)
{
    return (std::max)(0, (std::min)(BGE_PLAYER_ICON_VISIBILITY_MODE_COUNT - 1, modeIndex));
}

inline int BgePlayerIconVisibilityModeIndexFromKey(unsigned int key)
{
    if (key >= static_cast<unsigned int>(L'0') && key <= static_cast<unsigned int>(L'9')) {
        return static_cast<int>(key - static_cast<unsigned int>(L'0'));
    }
    constexpr unsigned int kVirtualKeyNumpad0 = 0x60;
    constexpr unsigned int kVirtualKeyNumpad9 = 0x69;
    if (key >= kVirtualKeyNumpad0 && key <= kVirtualKeyNumpad9) {
        return static_cast<int>(key - kVirtualKeyNumpad0);
    }
    return -1;
}

inline const BgePlayerIconVisibilityMode& BgePlayerIconVisibilityModeForIndex(int modeIndex)
{
    return BgePlayerIconVisibilityModes()[static_cast<std::size_t>(BgeNormalizePlayerIconVisibilityModeIndex(modeIndex))];
}

inline const wchar_t* BgePlayerIconVisibilityModeName(int modeIndex)
{
    return BgePlayerIconVisibilityModeForIndex(modeIndex).name;
}

inline float BgePlayerIconVisibilityModeAlpha(int modeIndex, bool invulnerable, bool lowBlinkPhase)
{
    const BgePlayerIconVisibilityMode& mode = BgePlayerIconVisibilityModeForIndex(modeIndex);
    if (!invulnerable) {
        return mode.alpha;
    }
    return lowBlinkPhase ? mode.invulnerableAlphaLow : mode.invulnerableAlphaHigh;
}

inline void BgeApplyPlayerIconVisibilityMode(BgeObjectSlotState& slot, int modeIndex, bool invulnerable)
{
    const BgePlayerIconVisibilityMode& mode = BgePlayerIconVisibilityModeForIndex(modeIndex);
    slot.shape = mode.shape;
    slot.renderStyle = mode.renderStyle;
    slot.radius = mode.radius;
    slot.outlineThickness = mode.outlineThickness;
    slot.colorR = mode.colorR;
    slot.colorG = mode.colorG;
    slot.colorB = mode.colorB;
    slot.colorA = BgePlayerIconVisibilityModeAlpha(modeIndex, invulnerable, false);
}

inline bool BgeObjectSlotsOverlap(const BgeObjectSlotState& first, const BgeObjectSlotState& second)
{
    if (!first.visible || !second.visible || first.isDeleted || second.isDeleted) {
        return false;
    }

    float deltaX = first.x - second.x;
    float deltaY = first.y - second.y;
    float radiusSum = first.radius + second.radius;
    return (deltaX * deltaX + deltaY * deltaY) <= (radiusSum * radiusSum);
}

template <std::size_t SlotCount>
inline void BgeUpdateCollisionFlags(std::array<BgeObjectSlotState, SlotCount>& slots)
{
    for (auto& slot : slots) {
        slot.collisionDetected = false;
    }

    for (std::size_t firstIndex = 0; firstIndex < SlotCount; ++firstIndex) {
        for (std::size_t secondIndex = firstIndex + 1; secondIndex < SlotCount; ++secondIndex) {
            if (BgeObjectSlotsOverlap(slots[firstIndex], slots[secondIndex])) {
                slots[firstIndex].collisionDetected = true;
                slots[secondIndex].collisionDetected = true;
            }
        }
    }
}

bool LoadBackgroundImageMesh(const std::wstring& path, std::vector<BgeColorVertex>& vertices, std::wstring& error);

inline std::array<unsigned char, 7> BgeSceneGlyphRows(wchar_t glyph)
{
    if (glyph >= L'a' && glyph <= L'z') {
        glyph = static_cast<wchar_t>(glyph - L'a' + L'A');
    }

    switch (glyph) {
    case L'0': return { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E };
    case L'1': return { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E };
    case L'2': return { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F };
    case L'3': return { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E };
    case L'4': return { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 };
    case L'5': return { 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E };
    case L'6': return { 0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E };
    case L'7': return { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 };
    case L'8': return { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E };
    case L'9': return { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E };
    case L'A': return { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
    case L'B': return { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E };
    case L'C': return { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E };
    case L'D': return { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E };
    case L'E': return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
    case L'F': return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 };
    case L'G': return { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F };
    case L'H': return { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
    case L'I': return { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E };
    case L'J': return { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0C };
    case L'K': return { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
    case L'L': return { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F };
    case L'M': return { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 };
    case L'N': return { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
    case L'O': return { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
    case L'P': return { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 };
    case L'Q': return { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D };
    case L'R': return { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 };
    case L'S': return { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E };
    case L'T': return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
    case L'U': return { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
    case L'V': return { 0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04 };
    case L'W': return { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 };
    case L'X': return { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 };
    case L'Y': return { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 };
    case L'Z': return { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F };
    case L'-': return { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 };
    case L':': return { 0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00 };
    case L'/': return { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 };
    case L'.': return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C };
    default:   return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    }
}

inline void BgeAppendSceneOverlayText(BgeColorVertex* vertices, int& vertexCount, int maxVertexCount, const BgeSceneOverlayText& overlay, float targetWidth, float targetHeight)
{
    if (overlay.text.empty() || overlay.scale <= 0.0f || targetWidth <= 0.0f || targetHeight <= 0.0f) {
        return;
    }

    float advance = overlay.scale * 6.0f;
    float textWidth = overlay.text.empty() ? 0.0f : (static_cast<float>(overlay.text.size()) * advance) - overlay.scale;
    float startX = overlay.centered ? overlay.x - textWidth * 0.5f : overlay.x;
    float startY = overlay.y;

    auto appendQuad = [&](float left, float top) {
        if (vertexCount + 6 > maxVertexCount) {
            return;
        }

        float right = left + overlay.scale;
        float bottom = top + overlay.scale;
        BgeColorVertex topLeft{ (left / targetWidth) * 2.0f - 1.0f, 1.0f - (top / targetHeight) * 2.0f, overlay.r, overlay.g, overlay.b, overlay.a };
        BgeColorVertex topRight{ (right / targetWidth) * 2.0f - 1.0f, 1.0f - (top / targetHeight) * 2.0f, overlay.r, overlay.g, overlay.b, overlay.a };
        BgeColorVertex bottomLeft{ (left / targetWidth) * 2.0f - 1.0f, 1.0f - (bottom / targetHeight) * 2.0f, overlay.r, overlay.g, overlay.b, overlay.a };
        BgeColorVertex bottomRight{ (right / targetWidth) * 2.0f - 1.0f, 1.0f - (bottom / targetHeight) * 2.0f, overlay.r, overlay.g, overlay.b, overlay.a };

        // Wind clockwise in screen space so the quad is front-facing in
        // D3D's left-handed NDC after the y-flip. Previous CCW order was
        // silently culled by the default rasterizer state, making all
        // overlay text invisible while it was correctly being appended.
        vertices[vertexCount++] = topLeft;
        vertices[vertexCount++] = topRight;
        vertices[vertexCount++] = bottomLeft;
        vertices[vertexCount++] = topRight;
        vertices[vertexCount++] = bottomRight;
        vertices[vertexCount++] = bottomLeft;
    };

    for (std::size_t characterIndex = 0; characterIndex < overlay.text.size(); ++characterIndex) {
        std::array<unsigned char, 7> rows = BgeSceneGlyphRows(overlay.text[characterIndex]);
        float glyphX = startX + static_cast<float>(characterIndex) * advance;
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[static_cast<std::size_t>(row)] & (1 << (4 - column))) == 0) {
                    continue;
                }
                appendQuad(glyphX + static_cast<float>(column) * overlay.scale, startY + static_cast<float>(row) * overlay.scale);
            }
        }
    }
}

// Append a closed (or open) polyline as filled stroke quads, one per edge.
// `points` are in screen-space pixels; `targetWidth`/`targetHeight` give the
// NDC transform. `thickness` is in screen-space pixels (total width).
// Quad winding matches BgeAppendSceneOverlayText so it survives the default
// D3D left-handed back-face cull.
inline void BgeAppendStrokedPolygon(BgeColorVertex* vertices, int& vertexCount, int maxVertexCount,
                                    const float* xs, const float* ys, int pointCount, bool closed,
                                    float thickness, float r, float g, float b, float a,
                                    float targetWidth, float targetHeight)
{
    if (pointCount < 2 || thickness <= 0.0f || targetWidth <= 0.0f || targetHeight <= 0.0f) {
        return;
    }
    float half = thickness * 0.5f;
    int edgeCount = closed ? pointCount : pointCount - 1;
    auto ndcX = [targetWidth](float x) { return (x / targetWidth) * 2.0f - 1.0f; };
    auto ndcY = [targetHeight](float y) { return 1.0f - (y / targetHeight) * 2.0f; };
    for (int i = 0; i < edgeCount; ++i) {
        if (vertexCount + 6 > maxVertexCount) {
            return;
        }
        int j = (i + 1) % pointCount;
        float dx = xs[j] - xs[i];
        float dy = ys[j] - ys[i];
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.0001f) {
            continue;
        }
        float ux = dx / len;
        float uy = dy / len;
        float nx = -uy;
        float ny = ux;
        float ax = xs[i] + nx * half;
        float ay = ys[i] + ny * half;
        float bx = xs[i] - nx * half;
        float by = ys[i] - ny * half;
        float cx = xs[j] + nx * half;
        float cy = ys[j] + ny * half;
        float dxq = xs[j] - nx * half;
        float dyq = ys[j] - ny * half;
        BgeColorVertex va{ ndcX(ax), ndcY(ay), r, g, b, a };
        BgeColorVertex vb{ ndcX(bx), ndcY(by), r, g, b, a };
        BgeColorVertex vc{ ndcX(cx), ndcY(cy), r, g, b, a };
        BgeColorVertex vd{ ndcX(dxq), ndcY(dyq), r, g, b, a };
        // CW in screen space → front-facing in D3D NDC after y-flip.
        vertices[vertexCount++] = va;
        vertices[vertexCount++] = vc;
        vertices[vertexCount++] = vb;
        vertices[vertexCount++] = vc;
        vertices[vertexCount++] = vd;
        vertices[vertexCount++] = vb;
    }
}