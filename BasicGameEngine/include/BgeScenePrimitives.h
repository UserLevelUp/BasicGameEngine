#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

constexpr int BGE_OBJECT_SLOT_COUNT = 10;

enum class BgeEdgePolicy : int {
    Bounce = 0,
    Wrap = 1,
    Clamp = 2,
};

enum class BgeObjectShape : int {
    Ball = 0,
    Asteroid = 1,
};

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

inline BgeEdgePolicy BgeCurrentEdgePolicy()
{
    return static_cast<BgeEdgePolicy>(BgeEdgePolicyStorage().load(std::memory_order_relaxed));
}

inline void BgeSetCurrentEdgePolicy(BgeEdgePolicy policy)
{
    BgeEdgePolicyStorage().store(static_cast<int>(policy), std::memory_order_relaxed);
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
    case BgeObjectShape::Asteroid: return L"asteroid";
    case BgeObjectShape::Ball:
    default:                       return L"ball";
    }
}

inline bool BgeTryParseObjectShape(const std::wstring& text, BgeObjectShape& out)
{
    if (text == L"ball" || text == L"circle") { out = BgeObjectShape::Ball; return true; }
    if (text == L"asteroid" || text == L"rock") { out = BgeObjectShape::Asteroid; return true; }
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
    BgeObjectShape shape = BgeObjectShape::Ball;
    BgeObjectKind kind = BgeObjectKind::Generic;
};

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