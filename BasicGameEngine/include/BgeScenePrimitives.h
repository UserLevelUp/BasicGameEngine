#pragma once

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
    float x = 180.0f;
    float y = 180.0f;
    float velocityX = 180.0f;
    float velocityY = 135.0f;
    float radius = 32.0f;
    float colorR = 0.96f;
    float colorG = 0.34f;
    float colorB = 0.22f;
};

bool LoadBackgroundImageMesh(const std::wstring& path, std::vector<BgeColorVertex>& vertices, std::wstring& error);