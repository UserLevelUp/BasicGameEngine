#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

constexpr int BGE_OBJECT_SLOT_COUNT = 10;

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