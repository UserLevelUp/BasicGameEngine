#pragma once

#include "BgeScenePrimitives.h"

#include <array>
#include <mutex>
#include <string>
#include <vector>

struct BgeGameViewport {
    float width = 640.0f;
    float height = 480.0f;
    float playTop = 0.0f;
    float playHeight = 480.0f;
};

struct BgeGameRuntime {
    bool (*ownsGameLoop)() = nullptr;
    BgeGameViewport (*viewport)() = nullptr;

    std::mutex* objectMutex = nullptr;
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT>* objectSlots = nullptr;
    bool* animationRunning = nullptr;
    int* activeObjectGroupIndex = nullptr;
    int* mainPlayerGroupIndex = nullptr;
    int* mainPlayerSlot = nullptr;
    int* selectedObjectSlot = nullptr;
    bool* objectSelectionActive = nullptr;
    bool* rendererStateDirty = nullptr;

    void (*setObjectKeyboardFocusLocked)() = nullptr;
    void (*refreshSelectedObjectGlobalsLocked)() = nullptr;
    void (*persistActiveObjectGroupLocked)() = nullptr;
    void (*syncControls)() = nullptr;
    void (*invalidateRenderer)() = nullptr;
    void (*setStatus)(const std::wstring& statusText) = nullptr;
    void (*setHud)(const std::wstring& hudText) = nullptr;
    void (*log)(const std::string& message) = nullptr;
};

class BgeGameModule {
public:
    virtual ~BgeGameModule() = default;

    virtual const wchar_t* Name() const = 0;
    virtual bool OnStart(BgeGameRuntime& runtime, std::wstring& statusText) = 0;
    virtual bool OnCommand(BgeGameRuntime& runtime, const std::vector<std::wstring>& tokens, std::wstring& statusText) = 0;
    virtual bool OnKeyDown(BgeGameRuntime& runtime, unsigned int key) = 0;
    virtual bool OnTick(BgeGameRuntime& runtime, double deltaMilliseconds) = 0;
};

BgeGameModule& BgeAsteroidGameModule();