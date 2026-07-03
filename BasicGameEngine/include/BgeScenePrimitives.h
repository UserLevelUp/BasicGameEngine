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
constexpr int BGE_UFO_VIEW_MODE_COUNT = 10;
constexpr int BGE_TRIAL_GROUP_COUNT = 2;

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
    Ufo = 4,
    Runner = 5,
    Quipu = 6,
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
    Ufo = 4,
    Runner = 5,
    Quipu = 6,
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

enum class BgeTrialGroup : int {
    PlayerShip = 0,
    Ufo = 1,
};
inline std::atomic<int>& BgeRenderTopInsetTenthsStorage()
{
    static std::atomic<int> insetTenths{ 1400 };
    return insetTenths;
}

inline int BgeNormalizeTrialGroupIndex(int groupIndex)
{
    return (std::max)(0, (std::min)(BGE_TRIAL_GROUP_COUNT - 1, groupIndex));
}

inline BgeTrialGroup BgeNormalizeTrialGroup(BgeTrialGroup group)
{
    return static_cast<BgeTrialGroup>(BgeNormalizeTrialGroupIndex(static_cast<int>(group)));
}

inline BgeTrialGroup BgeCycleTrialGroup(BgeTrialGroup group, int delta)
{
    int next = static_cast<int>(group) + delta;
    while (next < 0) {
        next += BGE_TRIAL_GROUP_COUNT;
    }
    next %= BGE_TRIAL_GROUP_COUNT;
    return static_cast<BgeTrialGroup>(next);
}

inline const wchar_t* BgeTrialGroupName(BgeTrialGroup group)
{
    switch (BgeNormalizeTrialGroup(group)) {
    case BgeTrialGroup::Ufo:        return L"ufo";
    case BgeTrialGroup::PlayerShip:
    default:                        return L"ship";
    }
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
    case BgeObjectShape::Quipu:  return L"quipu";
    case BgeObjectShape::Runner: return L"runner";
    case BgeObjectShape::Line: return L"line";
    case BgeObjectShape::Ufo: return L"ufo";
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
    if (text == L"ufo" || text == L"saucer") { out = BgeObjectShape::Ufo; return true; }
    if (text == L"runner" || text == L"chasqui") { out = BgeObjectShape::Runner; return true; }
    if (text == L"quipu" || text == L"knot" || text == L"knots") { out = BgeObjectShape::Quipu; return true; }
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
    case BgeObjectKind::Ufo:      return L"ufo";
    case BgeObjectKind::Runner:   return L"runner";
    case BgeObjectKind::Quipu:    return L"quipu";
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
    bool spriteEnabled = false;
    std::wstring spriteImagePath;
    float spriteU0 = 0.0f;
    float spriteV0 = 0.0f;
    float spriteU1 = 1.0f;
    float spriteV1 = 1.0f;
};

struct BgeUfoViewMode {
    const wchar_t* name;
    float xFraction;
    float yFraction;
    float radius;
    float velocityX;
    float velocityY;
    BgeObjectRenderStyle renderStyle;
    float outlineThickness;
    float colorR;
    float colorG;
    float colorB;
    float alpha;
};

inline const std::array<BgeUfoViewMode, BGE_UFO_VIEW_MODE_COUNT>& BgeUfoViewModes()
{
    static const std::array<BgeUfoViewMode, BGE_UFO_VIEW_MODE_COUNT> modes{{
        { L"left entry high",    0.10f, 0.22f, 20.0f,  170.0f,   0.0f, BgeObjectRenderStyle::Outline, 2.0f, 0.84f, 0.96f, 1.00f, 1.0f },
        { L"right entry high",   0.90f, 0.22f, 20.0f, -170.0f,   0.0f, BgeObjectRenderStyle::Outline, 2.0f, 0.84f, 0.96f, 1.00f, 1.0f },
        { L"top center hold",    0.50f, 0.22f, 22.0f,    0.0f,   0.0f, BgeObjectRenderStyle::Outline, 2.0f, 0.92f, 0.98f, 1.00f, 1.0f },
        { L"left middle drift",  0.12f, 0.50f, 20.0f,  130.0f,  16.0f, BgeObjectRenderStyle::Outline, 2.0f, 0.74f, 0.96f, 0.92f, 1.0f },
        { L"right middle drift", 0.88f, 0.50f, 20.0f, -130.0f, -16.0f, BgeObjectRenderStyle::Outline, 2.0f, 0.74f, 0.96f, 0.92f, 1.0f },
        { L"lower center hold",  0.50f, 0.78f, 22.0f,    0.0f,   0.0f, BgeObjectRenderStyle::Outline, 2.5f, 0.96f, 0.86f, 0.48f, 1.0f },
        { L"left edge clip",     0.04f, 0.35f, 22.0f,   80.0f,   0.0f, BgeObjectRenderStyle::Outline, 3.0f, 1.00f, 0.88f, 0.42f, 1.0f },
        { L"right edge clip",    0.96f, 0.65f, 22.0f,  -80.0f,   0.0f, BgeObjectRenderStyle::Outline, 3.0f, 1.00f, 0.88f, 0.42f, 1.0f },
        { L"large filled",       0.50f, 0.35f, 28.0f,    0.0f,   0.0f, BgeObjectRenderStyle::Filled,  2.0f, 0.58f, 0.88f, 1.00f, 1.0f },
        { L"small bright",       0.50f, 0.65f, 14.0f,    0.0f,   0.0f, BgeObjectRenderStyle::Outline, 1.5f, 0.94f, 1.00f, 0.72f, 1.0f },
    }};
    return modes;
}

inline int BgeNormalizeUfoViewModeIndex(int modeIndex)
{
    return (std::max)(0, (std::min)(BGE_UFO_VIEW_MODE_COUNT - 1, modeIndex));
}

inline const BgeUfoViewMode& BgeUfoViewModeForIndex(int modeIndex)
{
    return BgeUfoViewModes()[static_cast<std::size_t>(BgeNormalizeUfoViewModeIndex(modeIndex))];
}

inline const wchar_t* BgeUfoViewModeName(int modeIndex)
{
    return BgeUfoViewModeForIndex(modeIndex).name;
}

inline int BgeDigitModeIndexFromKey(unsigned int key)
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

inline void BgeApplyUfoViewMode(BgeObjectSlotState& slot, int modeIndex, float viewportWidth, float playTop, float playHeight)
{
    const BgeUfoViewMode& mode = BgeUfoViewModeForIndex(modeIndex);
    float width = (std::max)(1.0f, viewportWidth);
    float height = (std::max)(1.0f, playHeight);
    slot.visible = true;
    slot.deleteMarked = false;
    slot.isDeleted = false;
    slot.collisionDetected = false;
    slot.x = width * mode.xFraction;
    slot.y = playTop + height * mode.yFraction;
    slot.radius = mode.radius;
    slot.velocityX = mode.velocityX;
    slot.velocityY = mode.velocityY;
    slot.headingX = mode.velocityX < 0.0f ? -1.0f : 1.0f;
    slot.headingY = 0.0f;
    slot.colorR = mode.colorR;
    slot.colorG = mode.colorG;
    slot.colorB = mode.colorB;
    slot.colorA = mode.alpha;
    slot.shape = BgeObjectShape::Ufo;
    slot.kind = BgeObjectKind::Ufo;
    slot.renderStyle = mode.renderStyle;
    slot.outlineThickness = mode.outlineThickness;
}

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
    float headingOffsetDegrees;
    bool invertHeadingY;
};

inline const std::array<BgePlayerIconVisibilityMode, BGE_PLAYER_ICON_VISIBILITY_MODE_COUNT>& BgePlayerIconVisibilityModes()
{
    // Third-pass heading lineup. Marc picked the small filled white icon as
    // the readable baseline, so keep the look stable and vary only the heading
    // transform: screen-y direction, coarse offsets, and small +/-10 degree
    // nudges. This isolates "where is the nose / where do bullets go?" from
    // color and size changes.
    static const std::array<BgePlayerIconVisibilityMode, BGE_PLAYER_ICON_VISIBILITY_MODE_COUNT> modes{{
        { L"heading current",      BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f,   0.0f, false },
        { L"heading invert y",     BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f,   0.0f, true  },
        { L"heading plus 90",      BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f,  90.0f, false },
        { L"heading minus 90",     BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f, -90.0f, false },
        { L"heading 180",          BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f, 180.0f, false },
        { L"invert y plus 90",     BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f,  90.0f, true  },
        { L"invert y minus 90",    BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f, -90.0f, true  },
        { L"invert y 180",         BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f, 180.0f, true  },
        { L"heading fine minus 10", BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f, -10.0f, false },
        { L"heading fine plus 10",  BgeObjectShape::VectorShip, BgeObjectRenderStyle::Filled, 15.0f, 2.0f, 1.00f, 1.00f, 1.00f, 1.0f, 1.00f, 1.00f,  10.0f, false },
    }};
    return modes;
}

inline int BgeNormalizePlayerIconVisibilityModeIndex(int modeIndex)
{
    return (std::max)(0, (std::min)(BGE_PLAYER_ICON_VISIBILITY_MODE_COUNT - 1, modeIndex));
}

inline int BgePlayerIconVisibilityModeIndexFromKey(unsigned int key)
{
    return BgeDigitModeIndexFromKey(key);
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
    (void)lowBlinkPhase;
    return mode.invulnerableAlphaHigh;
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

inline void BgeApplyPlayerIconHeadingMode(BgeObjectSlotState& slot, float logicalHeadingX, float logicalHeadingY, int modeIndex)
{
    const BgePlayerIconVisibilityMode& mode = BgePlayerIconVisibilityModeForIndex(modeIndex);
    float headingX = logicalHeadingX;
    float headingY = mode.invertHeadingY ? -logicalHeadingY : logicalHeadingY;
    float length = std::sqrt(headingX * headingX + headingY * headingY);
    if (length < 0.001f) {
        headingX = 1.0f;
        headingY = 0.0f;
        length = 1.0f;
    }
    headingX /= length;
    headingY /= length;

    if (mode.headingOffsetDegrees != 0.0f) {
        float radians = mode.headingOffsetDegrees * 3.14159265358979323846f / 180.0f;
        float cosine = std::cos(radians);
        float sine = std::sin(radians);
        float rotatedX = headingX * cosine - headingY * sine;
        float rotatedY = headingX * sine + headingY * cosine;
        headingX = rotatedX;
        headingY = rotatedY;
    }

    slot.headingX = headingX;
    slot.headingY = headingY;
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
bool LoadImageRgbaPixels(const std::wstring& path, std::vector<std::uint8_t>& pixels, unsigned int& width, unsigned int& height, std::wstring& error);

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

inline void BgeAppendQuipuGlyph(BgeColorVertex* vertices, int& vertexCount, int maxVertexCount,
                                const BgeObjectSlotState& slot, bool ghost,
                                float targetWidth, float targetHeight)
{
    if (!slot.visible || slot.isDeleted || targetWidth <= 0.0f || targetHeight <= 0.0f) {
        return;
    }

    constexpr float pi = 3.14159265358979323846f;
    float progress = (std::max)(0.0f, (std::min)(1.0f, slot.velocityX));
    bool complete = slot.velocityY > 0.5f;
    bool current = slot.headingX > 0.5f;
    float cordLength = (std::max)(slot.radius * 4.0f, std::abs(slot.headingY));
    float knotRadius = (std::max)(4.0f, slot.radius) * (current ? 1.18f : 1.0f);
    float alpha = ghost ? (std::min)(0.34f, slot.colorA * 0.45f) : slot.colorA;
    float red = ghost ? 0.16f + slot.colorR * 0.16f : slot.colorR;
    float green = ghost ? 0.14f + slot.colorG * 0.16f : slot.colorG;
    float blue = ghost ? 0.10f + slot.colorB * 0.16f : slot.colorB;
    float thickness = (std::max)(1.2f, slot.outlineThickness) * (complete ? 1.30f : 1.0f);

    auto stroke = [&](const float* xs, const float* ys, int count, bool closed, float scale, float r, float g, float b, float a) {
        BgeAppendStrokedPolygon(vertices, vertexCount, maxVertexCount,
                                xs, ys, count, closed, thickness * scale,
                                r, g, b, a, targetWidth, targetHeight);
    };

    float topX = slot.x;
    float topY = slot.y;
    float bottomY = topY + cordLength;
    float cordX[2] = { topX, topX };
    float cordY[2] = { topY, bottomY };
    stroke(cordX, cordY, 2, false, current ? 1.20f : 0.92f, red, green, blue, alpha);

    float tieHalfWidth = knotRadius * (current ? 1.70f : 1.35f);
    float tieX[2] = { topX - tieHalfWidth, topX + tieHalfWidth };
    float tieY[2] = { topY, topY };
    stroke(tieX, tieY, 2, false, 1.10f, red, green, blue, alpha);

    if (complete || current) {
        float braidX[3] = { topX - knotRadius * 0.42f, topX + knotRadius * 0.34f, topX - knotRadius * 0.22f };
        float braidY[3] = { topY + cordLength * 0.18f, topY + cordLength * 0.33f, topY + cordLength * 0.50f };
        stroke(braidX, braidY, 3, false, 0.55f,
               (std::min)(1.0f, red + 0.14f), (std::min)(1.0f, green + 0.10f), (std::min)(1.0f, blue + 0.06f), alpha);
    }

    int knotCount = complete ? 5 : static_cast<int>(std::ceil(progress * 5.0f));
    if (current && knotCount == 0) {
        knotCount = 1;
    }
    knotCount = (std::max)(0, (std::min)(5, knotCount));

    for (int knotIndex = 0; knotIndex < knotCount; ++knotIndex) {
        float t = 0.22f + static_cast<float>(knotIndex) * 0.135f;
        float centerY = topY + cordLength * t;
        float centerX = topX + ((knotIndex % 2) == 0 ? -0.18f : 0.16f) * knotRadius;
        float rx = knotRadius * (complete ? 0.76f : 0.62f);
        float ry = knotRadius * (complete ? 0.46f : 0.38f);
        float knotX[10];
        float knotY[10];
        for (int point = 0; point < 10; ++point) {
            float angle = (static_cast<float>(point) / 10.0f) * 2.0f * pi;
            knotX[point] = centerX + std::cos(angle) * rx;
            knotY[point] = centerY + std::sin(angle) * ry;
        }
        stroke(knotX, knotY, 10, true, current && knotIndex == knotCount - 1 ? 1.28f : 1.0f,
               (std::min)(1.0f, red + 0.10f), (std::min)(1.0f, green + 0.08f), (std::min)(1.0f, blue + 0.04f), alpha);

        float wrapX[2] = { centerX - rx * 0.75f, centerX + rx * 0.75f };
        float wrapY[2] = { centerY + ry * 0.40f, centerY - ry * 0.42f };
        stroke(wrapX, wrapY, 2, false, 0.56f, red, green, blue, alpha);
    }
}

inline void BgeAppendRunnerGlyph(BgeColorVertex* vertices, int& vertexCount, int maxVertexCount,
                                 const BgeObjectSlotState& slot, bool ghost,
                                 float targetWidth, float targetHeight)
{
    if (!slot.visible || slot.isDeleted || targetWidth <= 0.0f || targetHeight <= 0.0f) {
        return;
    }

    constexpr float pi = 3.14159265358979323846f;
    float radius = (std::max)(6.0f, slot.radius) * (ghost ? 1.08f : 1.0f);
    float alpha = ghost ? (std::min)(0.36f, slot.colorA * 0.48f) : slot.colorA;
    float red = ghost ? 0.12f + slot.colorR * 0.18f : slot.colorR;
    float green = ghost ? 0.12f + slot.colorG * 0.18f : slot.colorG;
    float blue = ghost ? 0.14f + slot.colorB * 0.20f : slot.colorB;
    float thickness = (std::max)(1.2f, slot.outlineThickness) * (slot.renderStyle == BgeObjectRenderStyle::Filled ? 1.35f : 1.0f);
    float facing = std::abs(slot.headingX) > 0.001f ? (slot.headingX < 0.0f ? -1.0f : 1.0f) : (slot.velocityX < -1.0f ? -1.0f : 1.0f);
    float framePhase = std::abs(slot.velocityY) > 0.001f ? slot.velocityY : ((slot.x * 0.075f) + (slot.y * 0.031f));
    float phase = std::sin(framePhase);
    float counterPhase = std::cos(framePhase);

    auto stroke = [&](const float* xs, const float* ys, int count, bool closed, float scale) {
        BgeAppendStrokedPolygon(vertices, vertexCount, maxVertexCount,
                                xs, ys, count, closed, thickness * scale,
                                red, green, blue, alpha, targetWidth, targetHeight);
    };

    float headRadius = radius * 0.25f;
    float headCenterX = slot.x + facing * radius * 0.08f;
    float headCenterY = slot.y - radius * 0.98f;
    float headX[8];
    float headY[8];
    for (int index = 0; index < 8; ++index) {
        float angle = (static_cast<float>(index) / 8.0f) * 2.0f * pi;
        headX[index] = headCenterX + std::cos(angle) * headRadius;
        headY[index] = headCenterY + std::sin(angle) * headRadius;
    }
    stroke(headX, headY, 8, true, 0.92f);

    float neckX = slot.x + facing * radius * 0.10f;
    float neckY = slot.y - radius * 0.62f;
    float hipX = slot.x - facing * radius * 0.08f;
    float hipY = slot.y + radius * 0.18f;
    float torsoX[2] = { neckX, hipX };
    float torsoY[2] = { neckY, hipY };
    stroke(torsoX, torsoY, 2, false, 1.25f);

    float shoulderX = slot.x + facing * radius * 0.02f;
    float shoulderY = slot.y - radius * 0.42f;
    float frontHandX = shoulderX + facing * radius * (0.42f + 0.24f * phase);
    float frontHandY = shoulderY + radius * (0.26f - 0.10f * counterPhase);
    float backHandX = shoulderX - facing * radius * (0.40f + 0.18f * phase);
    float backHandY = shoulderY + radius * (0.34f + 0.08f * counterPhase);
    float frontArmX[2] = { shoulderX, frontHandX };
    float frontArmY[2] = { shoulderY, frontHandY };
    float backArmX[2] = { shoulderX, backHandX };
    float backArmY[2] = { shoulderY, backHandY };
    stroke(frontArmX, frontArmY, 2, false, 0.86f);
    stroke(backArmX, backArmY, 2, false, 0.78f);

    float frontKneeX = hipX + facing * radius * (0.22f + 0.38f * phase);
    float frontKneeY = hipY + radius * (0.42f - 0.10f * counterPhase);
    float frontFootX = hipX + facing * radius * (0.58f + 0.42f * phase);
    float frontFootY = hipY + radius * (1.00f - 0.12f * counterPhase);
    float backKneeX = hipX - facing * radius * (0.24f + 0.34f * phase);
    float backKneeY = hipY + radius * (0.46f + 0.10f * counterPhase);
    float backFootX = hipX - facing * radius * (0.58f + 0.34f * phase);
    float backFootY = hipY + radius * (1.02f + 0.10f * counterPhase);
    float frontLegX[3] = { hipX, frontKneeX, frontFootX };
    float frontLegY[3] = { hipY, frontKneeY, frontFootY };
    float backLegX[3] = { hipX, backKneeX, backFootX };
    float backLegY[3] = { hipY, backKneeY, backFootY };
    stroke(frontLegX, frontLegY, 3, false, 1.02f);
    stroke(backLegX, backLegY, 3, false, 0.92f);

    float sashX[2] = { neckX - facing * radius * 0.20f, hipX + facing * radius * 0.25f };
    float sashY[2] = { neckY + radius * 0.12f, hipY - radius * 0.10f };
    BgeAppendStrokedPolygon(vertices, vertexCount, maxVertexCount,
                            sashX, sashY, 2, false, thickness * 0.55f,
                            (std::min)(1.0f, red + 0.18f), (std::min)(1.0f, green + 0.12f), (std::min)(1.0f, blue + 0.08f), alpha,
                            targetWidth, targetHeight);
}