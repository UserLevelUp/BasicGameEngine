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
    void (*setTitleScreen)(const std::wstring& text, const std::wstring& subtitle, const std::wstring& legend, const std::wstring& credit, bool blinkPrompt) = nullptr;
    void (*clearTitleScreen)() = nullptr;
    void (*log)(const std::string& message) = nullptr;

    // Engine-neutral 2D scene-geometry channel: a module may paint
    // arbitrary triangles (paths, trails, grids, overlays) that render
    // behind the BGE_OBJECT_SLOT_COUNT object slots. Vertices are in
    // normalized device coordinates (x,y in [-1,1], y up). Pass an empty
    // vector to clear. Does NOT consume object slots.
    void (*setSceneGeometry)(const std::vector<BgeColorVertex>& vertices) = nullptr;

    // Engine-neutral bound on the interactive vector drag (the "pull the
    // arrow" gesture that sets the selected object's velocity). A module may
    // cap how hard the arrow can be pulled (maxMagnitude, pixels/sec; <= 0 =
    // unlimited) and restrict its direction to a cone of angleHalfWidthRadians
    // around angleCenterRadians (<= 0 = unrestricted). This is a generic
    // bounded-vector control with no game semantics: a non-game domain could
    // use it to limit how far any object may be nudged. Defaults leave the
    // drag unlimited so controller authoring is unchanged.
    void (*setVectorDragLimit)(float maxMagnitude, float angleCenterRadians, float angleHalfWidthRadians) = nullptr;
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

// Hosted game plugin ABI. Game modules may ship as separate DLLs that the
// engine discovers at runtime. A plugin DLL exports two extern "C" entry
// points; the engine refuses to host the module unless the ABI versions
// match exactly.
//
//   unsigned int BgeGameModuleAbiVersion();
//   BgeGameModule* CreateBgeGameModule();
constexpr unsigned int BGE_GAME_MODULE_ABI_VERSION = 1;
using BgeGameModuleAbiVersionFn = unsigned int (*)();
using BgeCreateGameModuleFn = BgeGameModule* (*)();