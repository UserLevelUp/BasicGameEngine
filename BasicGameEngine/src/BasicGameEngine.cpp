// BasicGameEngine.cpp : Defines the entry point for the application.

#include "../include/StatusBarMgr.h"
#include "../framework.h"
#include "../include/BasicGameEngine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <commdlg.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <fstream>      // Pass A: bge.toml config reader
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <mutex> // For std::mutex and std::lock_guard
#include <vector>
#include "../resource.h"
#include <shellapi.h>
#include <windowsx.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include "../include/TaskBarMgr.h"
#include "../include/WindowMutexMgr.h"
#include "../include/InterprocessCommMgr.h"  // Include the InterprocessCommMgr header
#include "../include/SharedMemoryData.h"      // Include the shared memory struct
#include "../include/UserPrivilegeMgr.h"

#include "../../OpNode/OpNode.h"
#include "../include/BgeGameModule.h"
#include "../include/BgeAudioPluginOperation.h"
#include "../include/BgeBreakableRockFieldPluginOperation.h"
#include "../include/BgeProjectilePluginOperation.h"
#include "../include/BgeScenePrimitives.h"
#include "../include/BgeScoreboardPluginOperation.h"
#include "../include/BgeTitleScreenPluginOperation.h"
#include "../include/BgeVectorShipPluginOperation.h"
#include "../include/BasicGameRoleOperation.h"
#include "../include/DirectX11BouncingBallOperation.h"
#include "../include/DirectX11BouncingBallRenderer.h"
#include "../include/DirectX12BouncingBallRenderer.h"

UserPrivilegeMgr userPrivilegeMgr;
WindowMutexMgr windowMutexMgr; // Instance of the WindowMutexMgr
TaskBarMgr taskBarMgr; // Instance of the TaskBarMgr class
StatusBarMgr statusBarMgr; // Instance of the StatusBarMgr class

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

std::thread gameThread;        // Game loop thread
std::mutex gameLoopMutex;      // Mutex for synchronizing game loop
std::mutex ballConfigMutex;
bool isFocused = true;         // Indicates whether this instance is focused
bool shouldRun = true;         // Controls the main game loop
bool isPaused = false;         // Indicates whether the game is paused

double lineX = 10.0; // Initial X position of the line
double lineY = 10.0; // Initial Y position of the line
bool movingRight = true;       // Direction of movement
double speed = 0.1;            // Speed in pixels per millisecond (adjustable)

// Pass A: diagnostic toggles loaded from bge.toml (next to exe).
// Default true preserves existing behavior if bge.toml is absent.
bool g_showMutexLine = true;

std::wstring g_bgeSharedMemoryName = L"Local\\BasicGameEngineSharedMemory";
std::wstring g_bgeCoordMutexName = L"Local\\BasicGameEngineCoordMutex";

constexpr UINT IDM_BGE_LAUNCH_STACK = 41001;
constexpr UINT IDM_BGE_LAUNCH_GAME_LOOP = 41002;
constexpr UINT IDM_BGE_LAUNCH_SCENE_3D = 41003;
constexpr UINT IDM_BGE_LAUNCH_IMAGES = 41004;
constexpr UINT IDM_BGE_LAUNCH_SOUND = 41005;
constexpr UINT IDM_BGE_LAUNCH_SAMPLE_ONE = 41006;
constexpr UINT IDM_BGE_LAUNCH_SAMPLE_TWO = 41007;

constexpr int IDC_BGE_ADD_BALL = 42001;
constexpr int IDC_BGE_START_ANIMATION = 42002;
constexpr int IDC_BGE_RENDERER = 42003;
constexpr int IDC_BGE_VELOCITY_X = 42004;
constexpr int IDC_BGE_VELOCITY_Y = 42005;
constexpr int IDC_BGE_APPLY_VECTOR = 42006;
constexpr int IDC_BGE_COLOR_R = 42007;
constexpr int IDC_BGE_COLOR_G = 42008;
constexpr int IDC_BGE_COLOR_B = 42009;
constexpr int IDC_BGE_APPLY_COLOR = 42010;
constexpr int IDC_BGE_LOAD_BACKGROUND = 42011;
constexpr int IDC_BGE_STOP_ANIMATION = 42012;
constexpr int IDC_BGE_OPEN_MAPPING = 42013;
constexpr int IDC_BGE_EDIT_MODE_STATUS = 42014;
constexpr int IDC_BGE_MAPPING_TEXT = 42015;
constexpr int IDC_BGE_OBJECT_SLOT_BASE = 42100;
constexpr int IDC_BGE_SOUND_SLOT_BASE = 42200;
constexpr int IDC_BGE_COMMAND_EDIT = 42300;
constexpr int IDC_BGE_RUN_COMMAND = 42301;
constexpr int IDC_BGE_COMMAND_STATUS = 42302;
constexpr int IDC_BGE_CONTROLLER_TARGET_BASE = 42400;
constexpr int IDC_BGE_CONTROLLER_LAUNCH_BASE = 42500;
constexpr int IDC_BGE_CONTROLLER_INSPECT_BASE = 42600;
constexpr int IDC_BGE_CONTROLLER_HIDE_BASE = 42700;
constexpr int IDC_BGE_CONTROLLER_STATUS_BASE = 42800;
constexpr int IDC_BGE_CONTROLLER_TARGET_LABEL = 42900;
constexpr int IDC_BGE_CONTROLLER_RUNTIME_STATUS = 42901;
constexpr int IDC_BGE_CONTROLLER_HISTORY = 42902;
constexpr int IDC_BGE_CONTROLLER_HISTORY_DETAIL = 42903;
constexpr int IDC_BGE_OPEN_HISTORY = 42904;
constexpr int IDC_BGE_LOAD_ASTEROID_GAME = 42905;
constexpr int IDC_BGE_OBJECT_GROUP_COMBO = 42910;
constexpr int IDC_BGE_ADD_OBJECT_GROUP = 42911;
constexpr int IDC_BGE_GHOST_GROUP_COMBO = 42912;
constexpr int IDC_BGE_TOGGLE_GHOST = 42913;
constexpr int IDC_BGE_SET_PLAYER = 42914;
constexpr int IDC_BGE_PLAYER_STATUS = 42915;
constexpr int IDC_BGE_GAME_HUD_STATUS = 42916;
constexpr int IDC_BGE_ASTEROID_R = 42920;
constexpr int IDC_BGE_ASTEROID_G = 42921;
constexpr int IDC_BGE_ASTEROID_B = 42922;
constexpr int IDC_BGE_ASTEROID_A = 42923;
constexpr int IDC_BGE_ASTEROID_APPLY_RGBA = 42924;
constexpr int IDC_BGE_ASTEROID_APPLY_SHAPE = 42925;
constexpr int IDC_BGE_ASTEROID_STATUS = 42926;
constexpr int IDC_BGE_ASTEROID_GAME_PRESET = 42927;
constexpr int IDC_BGE_ASTEROID_GAME_WORKER = 42930;
constexpr int IDC_BGE_ASTEROID_FIRE = 42931;
constexpr int IDC_BGE_ASTEROID_HYPERSPACE = 42932;
constexpr int IDC_BGE_ASTEROID_PAUSE = 42933;
constexpr int IDC_BGE_ASTEROID_RESUME = 42934;
constexpr int IDC_BGE_ASTEROID_TITLE = 42935;
constexpr int IDC_BGE_ASTEROID_HUD = 42936;
constexpr int IDC_BGE_ASTEROID_COMMANDS = 42937;
constexpr ULONG_PTR BGE_COPYDATA_WORKER_COMMAND = 0xB6E00001;
constexpr ULONG_PTR BGE_COPYDATA_WORKER_TELEMETRY = 0xB6E00002;
constexpr int BGE_CONTROLLER_ARTIFACT_COUNT = 6;
constexpr size_t BGE_CONTROLLER_HISTORY_LIMIT = 128;
constexpr size_t BGE_DELETE_HISTORY_LIMIT = 64;
constexpr float BGE_RENDER_TOP_INSET = 140.0f;
constexpr int BGE_MIN_WORKER_CLIENT_WIDTH = 360;
constexpr float BGE_TRANSLATE_STEP = 12.0f;
constexpr float BGE_RESIZE_STEP = 4.0f;
constexpr float BGE_ROTATE_STEP_DEGREES = 5.0f;
constexpr float BGE_VECTOR_MAGNITUDE_STEP = 20.0f;
constexpr float BGE_MIN_OBJECT_RADIUS = 8.0f;
constexpr float BGE_MAX_OBJECT_RADIUS = 160.0f;
constexpr double BGE_ANIMATION_STEP_MILLISECONDS = 1000.0 / 60.0;
constexpr int BGE_EDIT_RATE_DEFAULT_INDEX = 2;
constexpr std::array<float, 5> BGE_EDIT_RATE_MULTIPLIERS = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
constexpr std::array<const wchar_t*, 5> BGE_EDIT_RATE_LABELS = { L"0.25x", L"0.5x", L"1x", L"2x", L"4x" };
const wchar_t kBgeHistoryWindowClass[] = L"BasicGameEngineHistoryWindow";
const wchar_t kBgeMappingWindowClass[] = L"BasicGameEngineMappingWindow";
const wchar_t kBgeAsteroidAlphaWindowClass[] = L"BasicGameEngineAsteroidAlphaWindow";
const wchar_t kBgeEmbeddedGameScriptResourceName[] = L"BGE_GAME_SCRIPT";
const wchar_t kBgeEmbeddedFeatureLedgerResourceName[] = L"BGE_FEATURE_LEDGER";

enum class BgeEditMode {
    Translate,
    Resize,
    Rotate,
};

enum class BgeKeyboardFocus {
    None,
    Object,
    Group,
    Animation,
};

struct BgeObjectGroupState {
    std::wstring label;
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> slots{};
    int selectedObjectSlot = 0;
    bool objectSelectionActive = true;
    bool deleteMarked = false;
    bool isDeleted = false;
};

struct BgeObjectDeleteSnapshot {
    int groupIndex = -1;
    int slotIndex = -1;
    BgeObjectSlotState slot;
};

struct BgeGroupDeleteSnapshot {
    int groupIndex = -1;
    bool deleteMarked = false;
    bool isDeleted = false;
};

struct BgeDeleteHistoryEntry {
    std::wstring label;
    int activeGroupIndex = 0;
    int selectedObjectSlot = 0;
    bool objectSelectionActive = false;
    std::vector<BgeObjectDeleteSnapshot> objectSnapshots;
    std::vector<BgeGroupDeleteSnapshot> groupSnapshots;
};

struct BgeControllerArtifactSpec {
    const wchar_t* group;
    const wchar_t* displayName;
    const wchar_t* role;
    const wchar_t* summary;
    bool visualByDefault;
};

struct BgePluginDescriptor {
    const wchar_t* id;
    const wchar_t* kind;
    const wchar_t* summary;
    const wchar_t* command;
    const wchar_t* flags;
    const wchar_t* emits;
};

struct BgeTitleScreenState {
    bool visible = false;
    std::wstring text = L"ASTEROIDS";
    std::wstring subtitle = L"PRESS ENTER";
    std::wstring start = L"Enter";
    std::wstring next = L"playing";
    std::wstring font = L"vector-5x7";
    std::wstring legend = L"20 LARGE ROCK   50 MEDIUM ROCK   100 SMALL ROCK   200 SAUCER";
    std::wstring credit = L"1 CREDIT  1 PLAY";
    bool centered = true;
    bool showLegend = true;
    bool showCredit = true;
    bool blinkPrompt = true;
};

struct BgeCounterState {
    std::wstring name;
    int value = 0;
    int minValue = 0;
    int maxValue = 0;
    bool hasMin = false;
    bool hasMax = false;
    bool persistSession = false;
};

struct BgeScoreboardState {
    bool visible = false;
    std::vector<std::wstring> counters;
    std::wstring anchor = L"top-left";
    std::wstring format = L"SCORE:{score}|HIGH:{high-score}|LIVES:{lives}|WAVE:{wave}";
    std::wstring font = L"vector-5x7";
    std::wstring color = L"vector-white";
    std::wstring iconRow;
    float scale = 2.0f;
};

struct BgeProjectileDefinition {
    std::wstring id = L"shot";
    std::wstring owner = L"player";
    BgeObjectShape shape = BgeObjectShape::Line;
    float speed = 620.0f;
    float ttlSeconds = 1.1f;
    bool wrap = true;
    std::wstring collisionTag = L"projectile";
};

struct BgeBreakableRockFieldState {
    bool active = false;
    std::vector<std::wstring> sizes = { L"large", L"medium", L"small" };
    int count = 4;
    int splitCount = 2;
    std::wstring score = L"score:20|50|100";
    float speedMin = 30.0f;
    float speedMax = 180.0f;
    std::wstring waveCounter = L"wave";
    std::wstring avoid = L"player_ship";
    float avoidRadius = 160.0f;
    BgeObjectRenderStyle renderStyle = BgeObjectRenderStyle::Filled;
    float outlineThickness = 2.0f;
};

struct BgeUfoState {
    bool active = false;
    std::wstring id = L"saucer";
    int slotIndex = -1;
    std::wstring score = L"score:200";
    std::wstring weapon = L"enemy-shot";
    std::wstring aim = L"player_ship";
    float arrivalMinSeconds = 7.0f;
    float arrivalMaxSeconds = 18.0f;
    float arrivalRemainingSeconds = 0.0f;
    float radius = 20.0f;
    float speed = 170.0f;
    bool edgeSpawn = true;
    BgeObjectRenderStyle renderStyle = BgeObjectRenderStyle::Outline;
    float outlineThickness = 2.0f;
};

struct BgeVectorShipState {
    bool active = false;
    std::wstring id = L"player_ship";
    int slotIndex = 0;
    BgeObjectShape shape = BgeObjectShape::VectorShip;
    std::wstring lives = L"lives";
    std::wstring inputProfile = L"arrows-space";
    std::wstring fire = L"shot";
    std::wstring hyperspace = L"H";
    float respawnSeconds = 1.5f;
    float invulnerableSeconds = 2.0f;
    float invulnerableRemainingSeconds = 0.0f;
    float respawnRemainingSeconds = 0.0f;
    bool awaitingRespawn = false;
    bool gameOver = false;
    float headingDegrees = -90.0f;
    float turnDegrees = 200.0f;
    float thrustStep = 280.0f;
    float reverseThrustStep = 187.0f;
    float drag = 0.012f;
    float maxSpeed = 420.0f;
    BgeObjectRenderStyle renderStyle = BgeObjectRenderStyle::Filled;
    float outlineThickness = 2.0f;
    int playerIconVisibilityMode = 0;
};

struct BgeVectorShipInputState {
    bool turnLeft = false;
    bool turnRight = false;
    bool thrust = false;
    bool reverse = false;
};

const std::array<BgeControllerArtifactSpec, BGE_CONTROLLER_ARTIFACT_COUNT> kControllerArtifacts = {{
    { L"Scene And Render", L"Game Loop", L"bge.game-loop", L"renderer, objects, animation", true },
    { L"Scene And Render", L"Scene 3D", L"bge.scene-3d", L"3D scene surface", false },
    { L"Scene And Render", L"Sample Game One", L"bge.sample-game-one", L"sample game plugin", false },
    { L"Scene And Render", L"Sample Game Two", L"bge.sample-game-two", L"sample game plugin", false },
    { L"Assets", L"Images", L"bge.images", L"backgrounds and image assets", false },
    { L"Audio", L"Sound", L"bge.sound", L"sound slots and playback", false },
}};

const std::array<BgePluginDescriptor, 8> kBgePluginRegistry = {{
    { L"bge.2d.arcade", L"capability", L"2D arcade viewport, counters, overlays, replay, and executable export command grammar", L"game define | viewport fit-host | counter define | export enable | export executable | inspect commands", L"--design-size --wrap-x --wrap-y --background --formats --target --name --include", L"bge.event.game.defined bge.event.scene.configured bge.event.replay.surface.enabled bge.event.export.requested" },
    { L"bge.piece.vector-ship", L"piece", L"Reusable vector player ship with input, firing, respawn, and hyperspace signature", L"player-ship create", L"--id --shape --lives --input-profile --fire --hyperspace --respawn --invulnerable", L"bge.event.entity.spawned bge.event.input.bound bge.event.player.respawned" },
    { L"bge.piece.breakable-rock-field", L"piece", L"Reusable breakable rock field with split, scoring, wave, and avoid-zone signature", L"rock-field create", L"--sizes --count --split --score --speed-range --wave-counter --avoid", L"bge.event.entity.spawned bge.event.rule.defined bge.event.counter.changed" },
    { L"bge.piece.projectile", L"piece", L"Reusable projectile with owner, speed, lifetime, wrap, and collision tag signature", L"projectile create", L"--id --owner --shape --speed --ttl --wrap --collision-tag", L"bge.event.entity.grammar.extended bge.event.entity.lifecycle.changed" },
    { L"bge.piece.ufo", L"piece", L"Reusable UFO enemy with score, weapon, arrival, aim, and edge-spawn signature", L"ufo create", L"--id --score --weapon --arrival --aim --edge-spawn", L"bge.event.entity.spawned bge.event.rule.fired bge.event.counter.changed" },
    { L"bge.piece.scoreboard", L"piece", L"Reusable vector scoreboard and HUD line bound to counters", L"scoreboard create", L"--counters --anchor --format --font --color --icon-row --scale", L"bge.event.overlay.defined bge.event.counter.observed" },
    { L"bge.piece.title-screen", L"piece", L"Reusable vector-arcade title screen with score legend, credit line, blinking prompt, and start command binding", L"title-screen create", L"--text --subtitle --start --next --font --center --legend --credit --blink", L"bge.event.screen.defined bge.event.command.bound bge.event.overlay.defined" },
    { L"bge.piece.audio", L"piece", L"Engine-neutral audio piece with synthesized-tone WAV playback via winmm; any domain (game, business-rules, DICOM alert) can consume it", L"sound define | sound play", L"--id --kind --freq --duration --volume", L"bge.event.sound.defined bge.event.sound.played" },
}};

const std::array<const wchar_t*, 8> kBgeAsteroidsPluginSet = {{
    L"bge.2d.arcade",
    L"bge.piece.vector-ship",
    L"bge.piece.breakable-rock-field",
    L"bge.piece.projectile",
    L"bge.piece.ufo",
    L"bge.piece.scoreboard",
    L"bge.piece.title-screen",
    L"bge.piece.audio",
}};

enum class BgeRendererApi {
    DirectX11,
    DirectX12,
};

DWORD g_currentPid = 0;
bool g_isController = false;
bool g_closeBroadcastSent = false;
std::wstring g_workerName = L"worker";
bool g_workerRoleRequested = false;
bool g_launchBasicGameStack = false;
bool g_playerRuntimeMode = false;
bool g_playerRuntimeCommandsProcessed = false;
std::wstring g_playerRuntimeTitle = L"Space Rocks";
std::wstring g_sessionCliName;
std::vector<std::wstring> g_launchRoles;
std::vector<std::wstring> g_workerCliArgs;
HANDLE g_coordMutex = nullptr;
SharedMemoryData* g_sharedData = nullptr;
DWORD64 g_lastHeartbeatTick = 0;
std::unique_ptr<DirectX11BouncingBallRenderer> g_directX11Renderer;
std::unique_ptr<DirectX12BouncingBallRenderer> g_directX12Renderer;
std::atomic<BgeRendererApi> g_rendererApi = BgeRendererApi::DirectX11;
bool g_ballAdded = false;
bool g_ballAnimationRunning = false;
bool g_rendererStateDirty = false;
bool g_rendererSwitchRequested = false;
bool g_rendererResizeRequested = false;
bool g_backgroundImageDirty = false;
bool g_draggingVectorTip = false;
float g_ballVelocityX = 180.0f;
float g_ballVelocityY = 135.0f;
float g_ballColorR = 0.96f;
float g_ballColorG = 0.34f;
float g_ballColorB = 0.22f;
float g_ballColorA = 1.0f;
int g_selectedObjectSlot = 0;
bool g_objectSelectionActive = true;
int g_activeSoundSlot = 0;
DWORD64 g_lastSoundSlotTick = 0;
std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> g_objectSlots;
std::vector<BgeObjectGroupState> g_objectGroups;
int g_activeObjectGroupIndex = 0;
int g_ghostObjectGroupIndex = -1;
bool g_ghostOverlayEnabled = false;
BgeKeyboardFocus g_keyboardFocus = BgeKeyboardFocus::None;
int g_mainPlayerGroupIndex = -1;
int g_mainPlayerSlot = -1;
std::vector<BgeDeleteHistoryEntry> g_deleteHistory;
bool g_titleScreenActive = false;
BgeTitleScreenState g_titleScreenState;
bool g_scoreboardActive = false;
BgeScoreboardState g_scoreboardState;
std::vector<BgeCounterState> g_bgeCounters;
bool g_breakableRockFieldActive = false;
BgeBreakableRockFieldState g_breakableRockFieldState;
bool g_ufoActive = false;
BgeUfoState g_ufoState;
std::vector<BgeProjectileDefinition> g_projectileDefinitions;
std::array<float, BGE_OBJECT_SLOT_COUNT> g_bgeProjectileLifeSeconds{};
bool g_vectorShipActive = false;
BgeVectorShipState g_vectorShipState;
BgeVectorShipInputState g_vectorShipInputState;
std::wstring g_backgroundImagePath;
HWND g_addBallButton = nullptr;
HWND g_startAnimationButton = nullptr;
HWND g_stopAnimationButton = nullptr;
HWND g_rendererCombo = nullptr;
HWND g_objectGroupCombo = nullptr;
HWND g_addObjectGroupButton = nullptr;
HWND g_ghostGroupCombo = nullptr;
HWND g_toggleGhostButton = nullptr;
HWND g_setPlayerButton = nullptr;
HWND g_playerStatus = nullptr;
HWND g_gameHudStatus = nullptr;
HWND g_asteroidGameWorkerButton = nullptr;
HWND g_asteroidFireButton = nullptr;
HWND g_asteroidHyperspaceButton = nullptr;
HWND g_asteroidPauseButton = nullptr;
HWND g_asteroidResumeButton = nullptr;
HWND g_asteroidTitleButton = nullptr;
HWND g_asteroidHudButton = nullptr;
HWND g_asteroidCommandsButton = nullptr;
HWND g_loadBackgroundButton = nullptr;
HWND g_openMappingButton = nullptr;
HWND g_editModeStatus = nullptr;
HWND g_velocityXEdit = nullptr;
HWND g_velocityYEdit = nullptr;
HWND g_applyVectorButton = nullptr;
HWND g_colorREdit = nullptr;
HWND g_colorGEdit = nullptr;
HWND g_colorBEdit = nullptr;
HWND g_applyColorButton = nullptr;
HWND g_commandEdit = nullptr;
HWND g_runCommandButton = nullptr;
HWND g_commandStatus = nullptr;
WNDPROC g_commandEditOriginalProc = nullptr;
std::array<HWND, BGE_OBJECT_SLOT_COUNT> g_objectSlotButtons{};
std::array<HWND, BGE_OBJECT_SLOT_COUNT> g_soundSlotButtons{};
HWND g_controllerTargetLabel = nullptr;
HWND g_controllerRuntimeStatus = nullptr;
HWND g_controllerHistoryList = nullptr;
HWND g_controllerHistoryDetail = nullptr;
HWND g_controllerHistoryWindow = nullptr;
HWND g_openHistoryButton = nullptr;
HWND g_loadAsteroidGameButton = nullptr;
HWND g_controllerHistoryListLabel = nullptr;
HWND g_controllerHistoryDetailLabel = nullptr;
HWND g_mappingWindow = nullptr;
HWND g_mappingText = nullptr;
HWND g_asteroidAlphaWindow = nullptr;
HWND g_asteroidREdit = nullptr;
HWND g_asteroidGEdit = nullptr;
HWND g_asteroidBEdit = nullptr;
HWND g_asteroidAEdit = nullptr;
HWND g_asteroidApplyRgbaButton = nullptr;
HWND g_asteroidApplyShapeButton = nullptr;
HWND g_asteroidStatus = nullptr;
std::array<HWND, BGE_CONTROLLER_ARTIFACT_COUNT> g_controllerTargetButtons{};
std::array<HWND, BGE_CONTROLLER_ARTIFACT_COUNT> g_controllerStatusLabels{};
std::array<HWND, BGE_CONTROLLER_ARTIFACT_COUNT> g_controllerLaunchButtons{};
std::array<HWND, BGE_CONTROLLER_ARTIFACT_COUNT> g_controllerInspectButtons{};
std::array<HWND, BGE_CONTROLLER_ARTIFACT_COUNT> g_controllerHideButtons{};
int g_selectedControllerArtifact = 0;
bool g_hideWorkerWindowsByDefault = true;
std::wstring g_cliControllerTarget;
std::vector<std::wstring> g_cliInspectSelectors;
std::vector<std::wstring> g_cliHideSelectors;
std::vector<std::wstring> g_cliConstructionArtifactPaths;
std::vector<std::wstring> g_revealedWorkerRoles;
std::vector<std::wstring> g_pendingControllerCommands;
std::vector<std::wstring> g_controllerHistory;
std::vector<std::wstring> g_controllerHistoryDetails;
std::vector<std::wstring> g_workerCommandHistory;
std::vector<std::wstring> g_importedBgePlugins;
BgeEditMode g_editMode = BgeEditMode::Translate;
int g_editRateIndex = BGE_EDIT_RATE_DEFAULT_INDEX;

HWND g_hWnd; // Global handle to the main window

// Interprocess communication manager
InterprocessCommMgr& commMgr = InterprocessCommMgr::GetInstance();
InterprocessComm* comm = nullptr;  // Pointer to the shared memory communication

// Function declarations
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    HistoryWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    MappingWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    AsteroidAlphaWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void GameLoop();               // Game loop function
void LoadConfig();             // Pass A: read bge.toml from the exe directory
void ParseRuntimeArgs();
void InitializeObjectSlots();
void InitializeObjectGroups();
bool RegisterCurrentProcess(SharedMemoryData* sharedData);
void UnregisterCurrentProcess();
void UpdateCurrentHeartbeat();
void RequestManagedWorkersClose();
bool EnsureCoordMutex();
bool IsProcessAlive(DWORD pid);
bool ConfirmControllerClose(HWND hWnd);
bool CloseWindowFromEscape(HWND sourceWindow);
bool HandleEscapeKey(HWND sourceWindow);
bool IsEditControl(HWND sourceWindow);
std::wstring LowerArg(std::wstring value);
bool TryParseFloatArg(const std::wstring& text, float& value);
bool CurrentProcessOwnsGameLoop();
bool CurrentProcessOwnsSoundLoop();
bool DirectXRendererActive();
bool AnyObjectSlotVisibleLocked();
int FirstHiddenObjectSlotLocked();
void RefreshSelectedObjectGlobalsLocked();
void SelectObjectSlotStateLocked(int slotIndex);
int ResolveObjectAddTargetLocked(int requestedSlotIndex);
void AddObjectSlotStateLocked(int requestedSlotIndex);
void ApplyObjectVectorStateLocked(float velocityX, float velocityY);
void ApplyObjectColorStateLocked(float colorR, float colorG, float colorB);
void ApplyObjectAlphaStateLocked(float alpha);
void ApplyObjectShapeStateLocked(BgeObjectShape shape);
void ApplyObjectAsteroidStateLocked(float colorR, float colorG, float colorB, float alpha, bool ensureVisible);
void RefreshObjectGroupGlobalsLocked();
void PersistActiveObjectGroupLocked();
bool SelectObjectGroupStateLocked(int groupIndex);
int AddObjectGroupStateLocked(const std::wstring& requestedLabel);
std::wstring ObjectGroupLabel(int groupIndex);
int ResolveObjectGroupIndex(const std::wstring& selector);
void PushDeleteHistoryLocked(const BgeDeleteHistoryEntry& entry);
bool ClearObjectSelectionLocked();
bool ClearObjectSelectionFromRendererClick(int x, int y);
bool ToggleSelectedObjectDeleteMark(std::wstring& statusText);
bool CommitMarkedObjectDeletes(std::wstring& statusText);
bool UndoLastDeleteAction(std::wstring& statusText);
bool HandleEditorShortcutKey(HWND sourceWindow, WPARAM key);
void InitializeSelectedRenderer(HWND hWnd);
void ShutdownActiveRenderer();
void ResizeActiveRenderer();
void TickActiveRenderer(double deltaMilliseconds);
void RenderActiveRenderer();
void ApplyBallStateToRenderer();
void LogRendererMessage(const std::string& message);
void LogRuntimeSceneState();
void LoadBackgroundOnActiveRenderer(const std::wstring& path);
void ProcessPendingRendererCommands();
void ShowAsteroidAlphaWindow();
void RefreshAsteroidAlphaWindow();
BgeGameViewport CurrentGameViewport();
BgeGameRuntime CreateGameRuntime();
bool ExecuteAsteroidGameModuleCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool StartAsteroidGameMode(std::wstring& statusText);
bool HandleAsteroidGameKeyDown(WPARAM key);
bool TickAsteroidGameMode(double deltaMilliseconds);
void AddBgePluginRegistryAttributesToOpNode(const std::shared_ptr<OpNode>& root);
bool BgePluginAlreadyImported(const std::wstring& pluginId);
bool ExecuteBgePluginCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeInspectCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeTitleScreenCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeCounterCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeScoreboardCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeBreakableRockFieldCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeProjectileCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeUfoCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeVectorShipCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool ExecuteBgeSoundCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool HandleBgeVectorShipKeyDown(WPARAM key);
bool HandleBgeVectorShipKeyUp(WPARAM key);
void ClearBgeVectorShipInputState();
void ApplyVectorShipHeadingModeLocked(BgeObjectSlotState& slot, const BgeVectorShipState& ship);
bool TickBgeProjectiles(double deltaMilliseconds);
bool TickBgeUfo(double deltaMilliseconds);
bool TickBgeVectorShip(double deltaMilliseconds);
bool VectorShipPlayerAliveLocked();
void ResolveBgeShipRockCollisionsLocked();
void RespawnVectorShipLocked();
bool SpawnSpaceRocksWaveLocked(int waveIndex);
void StartOrRestartSpaceRocksGameLocked();
void ShowSpaceRocksGameOverScreenLocked();
void ShowSpaceRocksTitleScreenLocked();
std::vector<BgeSceneOverlayText> BuildTitleScreenOverlays(const BgeTitleScreenState& titleScreen, const BgeGameViewport& viewport);
std::vector<BgeSceneOverlayText> BuildScoreboardOverlays(const BgeScoreboardState& scoreboard, const std::vector<BgeCounterState>& counters, const BgeGameViewport& viewport);
void ApplySceneOverlayTextToRenderer(const std::vector<BgeSceneOverlayText>& overlays);
void AddBallFromControls();
void StartAnimationFromControls();
void StopAnimationFromControls();
void ApplyVectorFromControls();
void ApplyColorFromControls();
void SwitchRendererFromControls();
void LoadBackgroundFromDialog();
void SelectObjectSlotFromControls(int slotIndex);
void SelectObjectGroupFromControls();
void AddObjectGroupFromControls();
void SelectGhostGroupFromControls();
void ToggleGhostGroupFromControls();
void SetMainPlayerFromControls();
void ExecuteAsteroidCommandFromControls(const wchar_t* commandText);
void SelectSoundSlotFromControls(int slotIndex);
void AdvanceSoundSlotLoop();
std::vector<std::wstring> TokenizeCommandText(const std::wstring& commandText);
void ExecuteCommandBarInput();
bool ExecuteCommandText(const std::wstring& commandText, std::wstring& statusText);
bool ExecuteControllerCommandText(const std::wstring& commandText, std::wstring& statusText);
bool ExecuteBgeExportCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText);
bool QueueConstructionArtifactCommandsFromFile(const std::wstring& path, std::wstring& statusText);
bool ExecuteConstructionCommandsLocally(const std::vector<std::wstring>& commands, const std::wstring& sourceLabel, std::wstring& statusText);
bool ExecuteConstructionArtifactCommandsLocally(const std::wstring& path, std::wstring& statusText);
bool LoadEmbeddedGameScriptCommands(std::vector<std::wstring>& commands, std::wstring& errorText);
bool SendControllerCommandToWorker(const std::wstring& role, const std::wstring& commandText, std::wstring& statusText);
void SendWorkerTelemetry(const std::wstring& kind, const std::wstring& commandText, const std::wstring& statusText);
void SetCommandStatus(const std::wstring& statusText);
void SetGameHudStatus(const std::wstring& hudText);
void AddControllerHistory(const std::wstring& historyText, const std::wstring& detailText = L"");
void UpdateControllerHistoryDetail(const std::wstring& detailText);
void UpdateControllerHistoryDetailFromSelection();
void ShowControllerHistoryWindow();
bool RegisterHistoryWindowClass();
void CreateControllerHistoryWindowControls(HWND hWnd);
void LayoutControllerHistoryWindow(HWND hWnd);
void RefreshControllerHistoryWindow();
void ShowMappingWindow();
bool RegisterMappingWindowClass();
void CreateMappingWindowControls(HWND hWnd);
void LayoutMappingWindow(HWND hWnd);
void RefreshMappingWindow();
std::wstring MappingWindowText();
std::wstring ControllerBaseCli();
std::wstring ControllerCommandDetail(const std::wstring& commandText);
std::wstring WorkerCommandDetail(const std::wstring& role, const std::wstring& commandText);
std::wstring TargetCommandDetail(const BgeControllerArtifactSpec& spec);
std::wstring LaunchCommandDetail(const std::wstring& role);
std::wstring InspectCommandDetail(const std::wstring& role);
std::wstring HideCommandDetail(const std::wstring& role);
std::wstring RoleShortName(const std::wstring& role);
int ArtifactIndexForRole(const std::wstring& role);
LRESULT CALLBACK CommandEditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool HandleWorkerCommandCopyData(COPYDATASTRUCT* copyData);
bool HandleControllerTelemetryCopyData(COPYDATASTRUCT* copyData);
bool TryStartVectorDrag(int x, int y);
bool TrySelectObjectAtPoint(int x, int y);
void UpdateVectorDrag(int x, int y);
void EndVectorDrag();
int ObjectSlotIndexFromNumberKey(WPARAM key);
bool SelectObjectSlotFromKeyboard(int slotIndex);
bool FocusObjectGroupsFromKeyboard();
bool CycleObjectGroupFromKeyboard(int direction);
bool FocusAnimationFromKeyboard();
bool StartAnimationState(std::wstring& statusText);
bool StopAnimationState(std::wstring& statusText);
bool StepAnimationOneTick(std::wstring& statusText);
bool HandleRendererKeyDown(WPARAM key);
bool HandleRendererKeyUp(WPARAM key);
void CycleEditMode(int direction);
bool SetEditModeFromText(const std::wstring& modeText);
std::wstring EditModeName(BgeEditMode mode);
float CurrentEditRateMultiplier();
std::wstring EditRateLabel();
void AdjustEditRate(int direction);
bool SetEditRateFromText(const std::wstring& rateText);
std::wstring TrimText(const std::wstring& value);
std::string Narrow(const std::wstring& value);
std::wstring WidenUtf8(const std::string& value);
std::wstring QuoteArg(const std::wstring& value);
std::wstring CommandFileArg(const std::wstring& value);
bool LoadConstructionArtifactCommands(const std::wstring& path, std::vector<std::wstring>& commands, std::wstring& errorText);
bool ExtractLineConstructionCommands(const std::wstring& text, std::vector<std::wstring>& commands);
bool ExtractJsonConstructionCommands(const std::wstring& text, std::vector<std::wstring>& commands, std::wstring& errorText);
bool ExtractCsvConstructionCommands(const std::wstring& text, std::vector<std::wstring>& commands, std::wstring& errorText);
void UpdateEditModeStatus();
bool TranslateSelectedObject(float deltaX, float deltaY, std::wstring& statusText);
bool ResizeSelectedObject(float deltaRadius, std::wstring& statusText);
bool RotateSelectedObject(float deltaDegrees, std::wstring& statusText);
bool AdjustSelectedObjectVectorMagnitude(float deltaMagnitude, std::wstring& statusText);
bool ApplySelectedObjectVectorIntent(float deltaDegrees, float deltaMagnitude, const std::wstring& actionName, std::wstring& statusText);
void CreateBallControls(HWND hWnd);
void CreateControllerControls(HWND hWnd);
void LayoutBallControls(HWND hWnd);
void SyncBallControls();
void SyncObjectGroupControls();
void SyncControllerControls();
void ProcessPlayerRuntimeAutomation();
void RecordWorkerCommandHistory(const std::wstring& commandText);
void SelectControllerArtifact(int artifactIndex, bool recordHistory = true);
void LaunchControllerArtifact(int artifactIndex);
void InspectControllerArtifact(int artifactIndex);
void HideControllerArtifact(int artifactIndex);
void LoadAsteroidGameFromController();
void ProcessControllerUiAutomation();
void AddControllerMenu(HWND hWnd);
void UpdateRoleWindowTitle(HWND hWnd);
void BootstrapRoleOpNode();
void LaunchWorkerRole(const std::wstring& role);
void LaunchBasicGameStack();
bool EnsureCoordMutex();
void PruneStaleWorkersLocked(SharedMemoryData* sharedData);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_currentPid = GetCurrentProcessId();
    InitializeObjectSlots();
    LoadConfig();  // Pass A: read bge.toml (toggles diagnostics; defaults preserved if absent)
    ParseRuntimeArgs();
    InitializeObjectGroups();

    // Try to create a new instance mutex
    std::wstring mutexName = windowMutexMgr.CreateInstanceMutex();
    if (mutexName.empty()) {
        // Mutex already exists, another instance is running
        MessageBox(NULL, L"Another instance is already running.", L"Basic Game Engine", MB_OK | MB_ICONWARNING);
        return 0;
    }

    // Initialize shared memory using InterprocessCommMgr
    comm = commMgr.CreateInterprocessComm(g_bgeSharedMemoryName.c_str(), sizeof(SharedMemoryData));
    if (!comm)
    {
        MessageBox(NULL, L"Could not create or access shared memory.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Update the status bar with the privilege status
    std::wstring privilegeStatus = userPrivilegeMgr.GetUserPrivilege().GetPrivilegeStatus();
    statusBarMgr.UpdatePrivilegeStatus(privilegeStatus);

    // Access the shared data structure
    SharedMemoryData* sharedData = comm->GetSharedMemoryPointer();
    if (sharedData)
    {
        //InterlockedIncrement(&sharedData->instanceCount); // Safely increment the instance count
        g_sharedData = sharedData;
        if (!RegisterCurrentProcess(sharedData)) {
            MessageBox(NULL, L"Could not register controller/worker state.", L"Error", MB_OK | MB_ICONERROR);
            return 0;
        }
    }
    else
    {
        MessageBox(NULL, L"Could not access shared memory.", L"Error", MB_OK | MB_ICONERROR);
        return 0; // Handle the error appropriately
    }

    // Initialize global strings, register class, etc.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BASICGAMEENGINE, szWindowClass, MAX_LOADSTRING);
    if (g_playerRuntimeMode && !g_playerRuntimeTitle.empty()) {
        wcsncpy_s(szTitle, g_playerRuntimeTitle.c_str(), _TRUNCATE);
    }
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    AddControllerMenu(g_hWnd);
    UpdateRoleWindowTitle(g_hWnd);
    CreateBallControls(g_hWnd);
    SyncBallControls();
    BootstrapRoleOpNode();
    InitializeSelectedRenderer(g_hWnd);
    ProcessPlayerRuntimeAutomation();
    LogRuntimeSceneState();

    if (g_isController) {
        if (g_launchBasicGameStack) {
            LaunchBasicGameStack();
        }
        for (const auto& role : g_launchRoles) {
            LaunchWorkerRole(role);
        }
    }

    // Update the instance count in the status bar
    if (!g_playerRuntimeMode && sharedData) {
        statusBarMgr.UpdateInstanceCount(sharedData->instanceCount); // Update status bar with instance count
    }

    // Start the per-process runtime loop. Only the bge.game-loop worker runs
    // frame/update work; other processes keep their shared-memory heartbeat.
    gameThread = std::thread(GameLoop);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BASICGAMEENGINE));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && HandleEscapeKey(msg.hwnd)) {
            continue;
        }
        if (msg.message == WM_KEYDOWN && HandleEditorShortcutKey(msg.hwnd, msg.wParam)) {
            continue;
        }
        if (msg.message == WM_KEYDOWN && !IsEditControl(msg.hwnd) && HandleRendererKeyDown(msg.wParam)) {
            continue;
        }
        if (msg.message == WM_KEYUP && !IsEditControl(msg.hwnd) && HandleRendererKeyUp(msg.wParam)) {
            continue;
        }
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Clean up when exiting
    if (g_isController && !g_closeBroadcastSent) {
        RequestManagedWorkersClose();
    }

    shouldRun = false; // Signal the thread to stop
    if (gameThread.joinable())
    {
        gameThread.join(); // Wait for the game thread to finish
    }

    ShutdownActiveRenderer();

    UnregisterCurrentProcess();
    windowMutexMgr.ReleaseInstanceMutex(mutexName); // Release the unique mutex
    commMgr.ReleaseInterprocessComm(g_bgeSharedMemoryName.c_str());

    if (g_coordMutex) {
        CloseHandle(g_coordMutex);
        g_coordMutex = nullptr;
    }

    return (int)msg.wParam;
}

// Pass A: minimal TOML-shaped config reader. Looks for `bge.toml` next to
// the running exe and pulls out a handful of `key = value` lines. Ignores
// sections and unknown keys. Pass B will swap this for toml++ when the
// schema grows (operation tree, factory, etc.).
void LoadConfig()
{
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return;
    std::wstring dir(exePath);
    size_t slash = dir.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return;
    std::wstring path = dir.substr(0, slash + 1) + L"bge.toml";

    std::ifstream file(path);
    if (!file) return;  // no config — keep defaults

    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t\r"));
        if (!s.empty()) s.erase(s.find_last_not_of(" \t\r") + 1);
    };

    std::string line;
    while (std::getline(file, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        trim(key);
        trim(val);
        // Strip surrounding double quotes from string values.
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }
        if (key == "show_mutex_line") {
            g_showMutexLine = (val == "true" || val == "1");
        }
        else if (key == "background_image") {
            g_backgroundImagePath = std::wstring(val.begin(), val.end());
            g_backgroundImageDirty = true;
        }
    }
}

void ResetObjectSlotArray(std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT>& slots, int groupIndex)
{
    const float colors[BGE_OBJECT_SLOT_COUNT][3] = {
        { 0.96f, 0.34f, 0.22f },
        { 0.20f, 0.72f, 0.95f },
        { 0.34f, 0.82f, 0.40f },
        { 0.95f, 0.78f, 0.22f },
        { 0.68f, 0.44f, 0.94f },
        { 0.95f, 0.42f, 0.72f },
        { 0.35f, 0.86f, 0.72f },
        { 0.92f, 0.50f, 0.20f },
        { 0.75f, 0.75f, 0.78f },
        { 0.55f, 0.82f, 0.24f },
    };

    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        BgeObjectSlotState& slot = slots[index];
        slot.visible = false;
        slot.deleteMarked = false;
        slot.isDeleted = false;
        slot.x = 180.0f + static_cast<float>(index) * 24.0f + static_cast<float>(groupIndex) * 16.0f;
        slot.y = 180.0f + static_cast<float>(index) * 10.0f + static_cast<float>(groupIndex % 4) * 18.0f;
        slot.velocityX = 150.0f + static_cast<float>(index) * 20.0f;
        slot.velocityY = (index % 2 == 0 ? 110.0f : -130.0f) + static_cast<float>(index) * 8.0f;
        slot.radius = 28.0f + static_cast<float>(index % 3) * 3.0f;
        slot.colorR = colors[index][0];
        slot.colorG = colors[index][1];
        slot.colorB = colors[index][2];
    }
}

void InitializeObjectSlots()
{
    ResetObjectSlotArray(g_objectSlots, 0);

    g_selectedObjectSlot = 0;
    g_objectSelectionActive = true;
    g_ballVelocityX = g_objectSlots[0].velocityX;
    g_ballVelocityY = g_objectSlots[0].velocityY;
    g_ballColorR = g_objectSlots[0].colorR;
    g_ballColorG = g_objectSlots[0].colorG;
    g_ballColorB = g_objectSlots[0].colorB;
}

void InitializeObjectGroups()
{
    BgeObjectGroupState firstGroup;
    firstGroup.label = L"Group 1";
    firstGroup.slots = g_objectSlots;
    firstGroup.selectedObjectSlot = g_selectedObjectSlot;
    firstGroup.objectSelectionActive = g_objectSelectionActive;
    g_objectGroups.clear();
    g_objectGroups.push_back(firstGroup);
    g_activeObjectGroupIndex = 0;
    g_ghostObjectGroupIndex = -1;
    g_ghostOverlayEnabled = false;
    g_mainPlayerGroupIndex = -1;
    g_mainPlayerSlot = -1;
    g_deleteHistory.clear();
}

std::wstring ObjectGroupLabel(int groupIndex)
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(g_objectGroups.size())) {
        return L"";
    }
    return g_objectGroups[static_cast<size_t>(groupIndex)].label;
}

void RefreshObjectGroupGlobalsLocked()
{
    if (g_objectGroups.empty()) {
        return;
    }

    g_activeObjectGroupIndex = (std::max)(0, (std::min)(g_activeObjectGroupIndex, static_cast<int>(g_objectGroups.size()) - 1));
    BgeObjectGroupState& activeGroup = g_objectGroups[static_cast<size_t>(g_activeObjectGroupIndex)];
    activeGroup.selectedObjectSlot = (std::max)(0, (std::min)(activeGroup.selectedObjectSlot, BGE_OBJECT_SLOT_COUNT - 1));
    g_objectSlots = activeGroup.slots;
    g_selectedObjectSlot = activeGroup.selectedObjectSlot;
    g_objectSelectionActive = activeGroup.objectSelectionActive;
    RefreshSelectedObjectGlobalsLocked();
    g_rendererStateDirty = true;
}

void PersistActiveObjectGroupLocked()
{
    if (g_objectGroups.empty() || g_activeObjectGroupIndex < 0 || g_activeObjectGroupIndex >= static_cast<int>(g_objectGroups.size())) {
        return;
    }

    BgeObjectGroupState& activeGroup = g_objectGroups[static_cast<size_t>(g_activeObjectGroupIndex)];
    activeGroup.slots = g_objectSlots;
    activeGroup.selectedObjectSlot = g_selectedObjectSlot;
    activeGroup.objectSelectionActive = g_objectSelectionActive;
}

bool SelectObjectGroupStateLocked(int groupIndex)
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(g_objectGroups.size())) {
        return false;
    }

    PersistActiveObjectGroupLocked();
    g_activeObjectGroupIndex = groupIndex;
    RefreshObjectGroupGlobalsLocked();
    g_keyboardFocus = BgeKeyboardFocus::Group;
    return true;
}

int AddObjectGroupStateLocked(const std::wstring& requestedLabel)
{
    PersistActiveObjectGroupLocked();

    int groupIndex = static_cast<int>(g_objectGroups.size());
    std::wstring label = TrimText(requestedLabel);
    if (label.empty()) {
        label = L"Group " + std::to_wstring(groupIndex + 1);
    }

    std::wstring baseLabel = label;
    int suffix = 2;
    bool labelExists = true;
    while (labelExists) {
        labelExists = false;
        for (const auto& group : g_objectGroups) {
            if (LowerArg(group.label) == LowerArg(label)) {
                labelExists = true;
                label = baseLabel + L" " + std::to_wstring(suffix++);
                break;
            }
        }
    }

    BgeObjectGroupState newGroup;
    newGroup.label = label;
    ResetObjectSlotArray(newGroup.slots, groupIndex);
    newGroup.selectedObjectSlot = 0;
    newGroup.objectSelectionActive = true;
    g_objectGroups.push_back(newGroup);
    g_activeObjectGroupIndex = groupIndex;
    RefreshObjectGroupGlobalsLocked();
    g_keyboardFocus = BgeKeyboardFocus::Group;
    return groupIndex;
}

int ResolveObjectGroupIndex(const std::wstring& selector)
{
    std::wstring cleanSelector = TrimText(selector);
    if (cleanSelector.empty()) {
        return g_activeObjectGroupIndex;
    }

    float numericValue = 0.0f;
    if (TryParseFloatArg(cleanSelector, numericValue)) {
        int oneBasedGroup = static_cast<int>(numericValue);
        if (oneBasedGroup >= 1 && oneBasedGroup <= static_cast<int>(g_objectGroups.size())) {
            return oneBasedGroup - 1;
        }
    }

    std::wstring selectorLower = LowerArg(cleanSelector);
    for (int groupIndex = 0; groupIndex < static_cast<int>(g_objectGroups.size()); ++groupIndex) {
        if (LowerArg(g_objectGroups[static_cast<size_t>(groupIndex)].label) == selectorLower) {
            return groupIndex;
        }
    }
    return -1;
}

void PushDeleteHistoryLocked(const BgeDeleteHistoryEntry& entry)
{
    if (entry.objectSnapshots.empty() && entry.groupSnapshots.empty()) {
        return;
    }

    g_deleteHistory.push_back(entry);
    if (g_deleteHistory.size() > BGE_DELETE_HISTORY_LIMIT) {
        g_deleteHistory.erase(g_deleteHistory.begin());
    }
}

BgeDeleteHistoryEntry MakeDeleteHistoryEntryLocked(const std::wstring& label)
{
    BgeDeleteHistoryEntry entry;
    entry.label = label;
    entry.activeGroupIndex = g_activeObjectGroupIndex;
    entry.selectedObjectSlot = g_selectedObjectSlot;
    entry.objectSelectionActive = g_objectSelectionActive;
    return entry;
}

void AddObjectDeleteSnapshotLocked(BgeDeleteHistoryEntry& entry, int groupIndex, int slotIndex)
{
    if (groupIndex < 0 || groupIndex >= static_cast<int>(g_objectGroups.size()) || slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
        return;
    }

    PersistActiveObjectGroupLocked();
    BgeObjectDeleteSnapshot snapshot;
    snapshot.groupIndex = groupIndex;
    snapshot.slotIndex = slotIndex;
    snapshot.slot = g_objectGroups[static_cast<size_t>(groupIndex)].slots[slotIndex];
    entry.objectSnapshots.push_back(snapshot);
}

bool ClearObjectSelectionLocked()
{
    if (!g_objectSelectionActive) {
        return false;
    }

    g_objectSelectionActive = false;
    g_keyboardFocus = BgeKeyboardFocus::None;
    PersistActiveObjectGroupLocked();
    g_rendererStateDirty = true;
    return true;
}

bool ClearObjectSelectionFromRendererClick(int x, int y)
{
    UNREFERENCED_PARAMETER(x);
    if (!CurrentProcessOwnsGameLoop() || y < static_cast<int>(BgeRenderTopInset())) {
        return false;
    }

    bool cleared = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_ballAnimationRunning) {
            return false;
        }
        cleared = ClearObjectSelectionLocked();
    }

    if (cleared) {
        SyncBallControls();
        SetCommandStatus(L"Object selection cleared; Delete commits marked objects");
    }
    return cleared;
}

bool ToggleSelectedObjectDeleteMark(std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"Delete commands run in bge.game-loop";
        return false;
    }

    int selectedSlot = 0;
    bool marked = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_ballAnimationRunning) {
            statusText = L"Stop animation before marking deletes";
            return false;
        }
        if (!g_objectSelectionActive) {
            statusText = L"No object selected; Delete commits marked objects";
            return false;
        }

        BgeObjectSlotState& slot = g_objectSlots[g_selectedObjectSlot];
        if (!slot.visible || slot.isDeleted) {
            statusText = L"Select a visible object";
            return false;
        }

        BgeDeleteHistoryEntry entry = MakeDeleteHistoryEntryLocked(slot.deleteMarked ? L"unmark object delete" : L"mark object delete");
        AddObjectDeleteSnapshotLocked(entry, g_activeObjectGroupIndex, g_selectedObjectSlot);
        PushDeleteHistoryLocked(entry);

        slot.deleteMarked = !slot.deleteMarked;
        selectedSlot = g_selectedObjectSlot;
        marked = slot.deleteMarked;
        g_rendererStateDirty = true;
        PersistActiveObjectGroupLocked();
    }

    SyncBallControls();
    statusText = std::wstring(marked ? L"Marked object " : L"Unmarked object ") + std::to_wstring(selectedSlot + 1) + L" for delete";
    return true;
}

bool CommitMarkedObjectDeletes(std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"Delete commands run in bge.game-loop";
        return false;
    }

    int deletedCount = 0;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_ballAnimationRunning) {
            statusText = L"Stop animation before deleting objects";
            return false;
        }

        BgeDeleteHistoryEntry entry = MakeDeleteHistoryEntryLocked(L"commit object deletes");
        for (int slotIndex = 0; slotIndex < BGE_OBJECT_SLOT_COUNT; ++slotIndex) {
            const BgeObjectSlotState& slot = g_objectSlots[slotIndex];
            if (slot.visible && !slot.isDeleted && slot.deleteMarked) {
                AddObjectDeleteSnapshotLocked(entry, g_activeObjectGroupIndex, slotIndex);
            }
        }

        if (entry.objectSnapshots.empty()) {
            statusText = L"No delete-marked objects";
            return false;
        }

        PushDeleteHistoryLocked(entry);
        for (const auto& snapshot : entry.objectSnapshots) {
            BgeObjectSlotState& slot = g_objectSlots[snapshot.slotIndex];
            slot.deleteMarked = false;
            slot.isDeleted = true;
            slot.visible = false;
            ++deletedCount;
            if (g_mainPlayerGroupIndex == g_activeObjectGroupIndex && g_mainPlayerSlot == snapshot.slotIndex) {
                g_mainPlayerGroupIndex = -1;
                g_mainPlayerSlot = -1;
            }
        }

        ClearObjectSelectionLocked();
        RefreshSelectedObjectGlobalsLocked();
        g_rendererStateDirty = true;
        PersistActiveObjectGroupLocked();
    }

    SyncBallControls();
    statusText = L"Deleted " + std::to_wstring(deletedCount) + L" marked object" + (deletedCount == 1 ? L"" : L"s");
    return true;
}

bool UndoLastDeleteAction(std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"Undo commands run in bge.game-loop";
        return false;
    }

    std::wstring actionLabel;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_deleteHistory.empty()) {
            statusText = L"Nothing to undo";
            return false;
        }

        PersistActiveObjectGroupLocked();
        BgeDeleteHistoryEntry entry = g_deleteHistory.back();
        g_deleteHistory.pop_back();
        actionLabel = entry.label;

        for (const auto& snapshot : entry.objectSnapshots) {
            if (snapshot.groupIndex < 0 || snapshot.groupIndex >= static_cast<int>(g_objectGroups.size()) || snapshot.slotIndex < 0 || snapshot.slotIndex >= BGE_OBJECT_SLOT_COUNT) {
                continue;
            }
            g_objectGroups[static_cast<size_t>(snapshot.groupIndex)].slots[snapshot.slotIndex] = snapshot.slot;
        }

        for (const auto& snapshot : entry.groupSnapshots) {
            if (snapshot.groupIndex < 0 || snapshot.groupIndex >= static_cast<int>(g_objectGroups.size())) {
                continue;
            }
            BgeObjectGroupState& group = g_objectGroups[static_cast<size_t>(snapshot.groupIndex)];
            group.deleteMarked = snapshot.deleteMarked;
            group.isDeleted = snapshot.isDeleted;
        }

        if (entry.activeGroupIndex >= 0 && entry.activeGroupIndex < static_cast<int>(g_objectGroups.size())) {
            g_activeObjectGroupIndex = entry.activeGroupIndex;
        }
        RefreshObjectGroupGlobalsLocked();
        g_selectedObjectSlot = (std::max)(0, (std::min)(entry.selectedObjectSlot, BGE_OBJECT_SLOT_COUNT - 1));
        g_objectSelectionActive = entry.objectSelectionActive;
        RefreshSelectedObjectGlobalsLocked();
        PersistActiveObjectGroupLocked();
        g_rendererStateDirty = true;
    }

    SyncBallControls();
    statusText = L"Undo: " + actionLabel;
    return true;
}

bool AnyObjectSlotVisibleLocked()
{
    for (const auto& slot : g_objectSlots) {
        if (slot.visible && !slot.isDeleted) {
            return true;
        }
    }
    return false;
}

int FirstHiddenObjectSlotLocked()
{
    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        if (!g_objectSlots[index].visible || g_objectSlots[index].isDeleted) {
            return index;
        }
    }
    return -1;
}

void RefreshSelectedObjectGlobalsLocked()
{
    const BgeObjectSlotState& slot = g_objectSlots[g_selectedObjectSlot];
    g_ballVelocityX = slot.velocityX;
    g_ballVelocityY = slot.velocityY;
    g_ballColorR = slot.colorR;
    g_ballColorG = slot.colorG;
    g_ballColorB = slot.colorB;
    g_ballColorA = slot.colorA;
    g_ballAdded = AnyObjectSlotVisibleLocked();
}

void SelectObjectSlotStateLocked(int slotIndex)
{
    g_selectedObjectSlot = (std::max)(0, (std::min)(BGE_OBJECT_SLOT_COUNT - 1, slotIndex));
    g_objectSelectionActive = true;
    g_keyboardFocus = BgeKeyboardFocus::Object;
    RefreshSelectedObjectGlobalsLocked();
    g_rendererStateDirty = true;
    PersistActiveObjectGroupLocked();
}

int ResolveObjectAddTargetLocked(int requestedSlotIndex)
{
    if (requestedSlotIndex >= 0 && requestedSlotIndex < BGE_OBJECT_SLOT_COUNT) {
        return requestedSlotIndex;
    }

    if (!g_objectSlots[g_selectedObjectSlot].visible) {
        return g_selectedObjectSlot;
    }

    int firstHidden = FirstHiddenObjectSlotLocked();
    return firstHidden >= 0 ? firstHidden : g_selectedObjectSlot;
}

void AddObjectSlotStateLocked(int requestedSlotIndex)
{
    int targetSlot = ResolveObjectAddTargetLocked(requestedSlotIndex);
    g_selectedObjectSlot = targetSlot;
    g_objectSlots[targetSlot].visible = true;
    g_objectSlots[targetSlot].deleteMarked = false;
    g_objectSlots[targetSlot].isDeleted = false;
    g_objectSelectionActive = true;
    RefreshSelectedObjectGlobalsLocked();
    g_rendererStateDirty = true;
    PersistActiveObjectGroupLocked();
}

void ApplyObjectShapeStateLocked(BgeObjectShape shape)
{
    g_objectSlots[g_selectedObjectSlot].shape = shape;
    g_rendererStateDirty = true;
    PersistActiveObjectGroupLocked();
}

void ApplyObjectVectorStateLocked(float velocityX, float velocityY)
{
    g_ballVelocityX = velocityX;
    g_ballVelocityY = velocityY;
    g_objectSlots[g_selectedObjectSlot].velocityX = velocityX;
    g_objectSlots[g_selectedObjectSlot].velocityY = velocityY;
    g_rendererStateDirty = true;
    PersistActiveObjectGroupLocked();
}

void ApplyObjectColorStateLocked(float colorR, float colorG, float colorB)
{
    g_ballColorR = (std::max)(0.0f, (std::min)(1.0f, colorR));
    g_ballColorG = (std::max)(0.0f, (std::min)(1.0f, colorG));
    g_ballColorB = (std::max)(0.0f, (std::min)(1.0f, colorB));
    g_objectSlots[g_selectedObjectSlot].colorR = g_ballColorR;
    g_objectSlots[g_selectedObjectSlot].colorG = g_ballColorG;
    g_objectSlots[g_selectedObjectSlot].colorB = g_ballColorB;
    g_rendererStateDirty = true;
    PersistActiveObjectGroupLocked();
}

void ApplyObjectAlphaStateLocked(float alpha)
{
    g_ballColorA = (std::max)(0.0f, (std::min)(1.0f, alpha));
    g_objectSlots[g_selectedObjectSlot].colorA = g_ballColorA;
    g_rendererStateDirty = true;
    PersistActiveObjectGroupLocked();
}

void ApplyObjectAsteroidStateLocked(float colorR, float colorG, float colorB, float alpha, bool ensureVisible)
{
    if (ensureVisible && (!g_objectSlots[g_selectedObjectSlot].visible || g_objectSlots[g_selectedObjectSlot].isDeleted)) {
        AddObjectSlotStateLocked(g_selectedObjectSlot);
    }
    ApplyObjectColorStateLocked(colorR, colorG, colorB);
    ApplyObjectAlphaStateLocked(alpha);
    ApplyObjectShapeStateLocked(BgeObjectShape::Asteroid);
    g_objectSlots[g_selectedObjectSlot].kind = BgeObjectKind::Asteroid;
    PersistActiveObjectGroupLocked();
}

float ClampFloat(float value, float minimumValue, float maximumValue)
{
    return (std::max)(minimumValue, (std::min)(maximumValue, value));
}

void ClampObjectToClientLocked(BgeObjectSlotState& slot)
{
    RECT client{};
    GetClientRect(g_hWnd, &client);
    float width = static_cast<float>((std::max)(client.right - client.left, 1L));
    float height = static_cast<float>((std::max)(client.bottom - client.top, 1L));
    slot.x = ClampFloat(slot.x, slot.radius, (std::max)(slot.radius, width - slot.radius));
    slot.y = ClampFloat(slot.y, BgeRenderTopInset() + slot.radius, (std::max)(BgeRenderTopInset() + slot.radius, height - slot.radius));
}

BgeGameViewport CurrentGameViewport()
{
    RECT client{};
    GetClientRect(g_hWnd, &client);
    BgeGameViewport viewport;
    viewport.width = static_cast<float>((std::max)(client.right - client.left, 1L));
    viewport.height = static_cast<float>((std::max)(client.bottom - client.top, 1L));
    viewport.playTop = BgeRenderTopInset();
    viewport.playHeight = (std::max)(1.0f, viewport.height - viewport.playTop);
    return viewport;
}

void SetGameObjectKeyboardFocusLocked()
{
    g_keyboardFocus = BgeKeyboardFocus::Object;
}

void InvalidateGameRenderer()
{
    InvalidateRect(g_hWnd, nullptr, FALSE);
}

void SetGameHudStatus(const std::wstring& hudText)
{
    if (g_gameHudStatus) {
        SetWindowTextW(g_gameHudStatus, hudText.c_str());
    }
}

BgeGameRuntime CreateGameRuntime()
{
    BgeGameRuntime runtime;
    runtime.ownsGameLoop = CurrentProcessOwnsGameLoop;
    runtime.viewport = CurrentGameViewport;
    runtime.objectMutex = &ballConfigMutex;
    runtime.objectSlots = &g_objectSlots;
    runtime.animationRunning = &g_ballAnimationRunning;
    runtime.activeObjectGroupIndex = &g_activeObjectGroupIndex;
    runtime.mainPlayerGroupIndex = &g_mainPlayerGroupIndex;
    runtime.mainPlayerSlot = &g_mainPlayerSlot;
    runtime.selectedObjectSlot = &g_selectedObjectSlot;
    runtime.objectSelectionActive = &g_objectSelectionActive;
    runtime.rendererStateDirty = &g_rendererStateDirty;
    runtime.setObjectKeyboardFocusLocked = SetGameObjectKeyboardFocusLocked;
    runtime.refreshSelectedObjectGlobalsLocked = RefreshSelectedObjectGlobalsLocked;
    runtime.persistActiveObjectGroupLocked = PersistActiveObjectGroupLocked;
    runtime.syncControls = SyncBallControls;
    runtime.invalidateRenderer = InvalidateGameRenderer;
    runtime.setStatus = SetCommandStatus;
    runtime.setHud = SetGameHudStatus;
    runtime.log = LogRendererMessage;
    return runtime;
}

bool ExecuteAsteroidGameModuleCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    BgeGameRuntime runtime = CreateGameRuntime();
    return BgeAsteroidGameModule().OnCommand(runtime, tokens, statusText);
}

bool StartAsteroidGameMode(std::wstring& statusText)
{
    BgeGameRuntime runtime = CreateGameRuntime();
    return BgeAsteroidGameModule().OnStart(runtime, statusText);
}

bool HandleAsteroidGameKeyDown(WPARAM key)
{
    BgeGameRuntime runtime = CreateGameRuntime();
    return BgeAsteroidGameModule().OnKeyDown(runtime, static_cast<unsigned int>(key));
}

bool TickAsteroidGameMode(double deltaMilliseconds)
{
    BgeGameRuntime runtime = CreateGameRuntime();
    return BgeAsteroidGameModule().OnTick(runtime, deltaMilliseconds);
}

bool TryGetCommandOptionValue(const std::vector<std::wstring>& tokens, const std::wstring& optionName, std::wstring& value)
{
    std::wstring optionPrefix = optionName + L"=";
    for (size_t tokenIndex = 0; tokenIndex < tokens.size(); ++tokenIndex) {
        if (tokens[tokenIndex] == optionName && tokenIndex + 1 < tokens.size()) {
            value = tokens[tokenIndex + 1];
            return true;
        }
        if (tokens[tokenIndex].rfind(optionPrefix, 0) == 0) {
            value = tokens[tokenIndex].substr(optionPrefix.size());
            return true;
        }
    }
    return false;
}

bool HasCommandFlag(const std::vector<std::wstring>& tokens, const std::wstring& flagName)
{
    return std::any_of(tokens.begin(), tokens.end(), [&flagName](const std::wstring& token) {
        return token == flagName;
    });
}

std::wstring NormalizeTitleScreenText(std::wstring value)
{
    std::wstring normalized;
    for (wchar_t ch : value) {
        if (ch == L'_') {
            normalized += L' ';
        }
        else if (ch == L'|') {
            normalized += L"   ";
        }
        else {
            normalized += ch;
        }
    }
    return normalized;
}

std::wstring TitleScreenStatusText(const BgeTitleScreenState& titleScreen)
{
    if (!titleScreen.visible) {
        return L"Title screen hidden";
    }
    return L"Title screen: " + titleScreen.text + L" | " + titleScreen.subtitle + L" | start " + titleScreen.start + L" -> " + titleScreen.next + (titleScreen.blinkPrompt ? L" | blink" : L"");
}

void AppendTitleScreenOverlay(std::vector<BgeSceneOverlayText>& overlays, const std::wstring& text, float x, float y, float scale, float r, float g, float b, float a, bool centered)
{
    if (text.empty()) {
        return;
    }
    BgeSceneOverlayText overlay;
    overlay.text = text;
    overlay.x = x;
    overlay.y = y;
    overlay.scale = scale;
    overlay.r = r;
    overlay.g = g;
    overlay.b = b;
    overlay.a = a;
    overlay.centered = centered;
    overlays.push_back(overlay);
}

std::vector<BgeSceneOverlayText> BuildTitleScreenOverlays(const BgeTitleScreenState& titleScreen, const BgeGameViewport& viewport)
{
    std::vector<BgeSceneOverlayText> overlays;
    if (!titleScreen.visible || viewport.width <= 1.0f || viewport.playHeight <= 1.0f) {
        return overlays;
    }

    float centerX = viewport.width * 0.5f;
    float titleScale = ClampFloat(viewport.width / 150.0f, 4.0f, 9.0f);
    float subtitleScale = ClampFloat(titleScale * 0.48f, 2.0f, 4.0f);
    float smallScale = ClampFloat(subtitleScale * 0.72f, 1.4f, 2.8f);
    float titleY = viewport.playTop + viewport.playHeight * 0.24f;
    float subtitleY = titleY + titleScale * 9.5f;

    AppendTitleScreenOverlay(overlays, titleScreen.text, centerX, titleY, titleScale, 0.82f, 0.92f, 1.0f, 1.0f, titleScreen.centered);
    AppendTitleScreenOverlay(overlays, titleScreen.subtitle, centerX, subtitleY, subtitleScale, 0.98f, 0.84f, 0.42f, 0.95f, titleScreen.centered);

    float legendY = subtitleY + subtitleScale * 9.5f;
    if (titleScreen.showLegend) {
        AppendTitleScreenOverlay(overlays, titleScreen.legend, centerX, legendY, smallScale, 0.62f, 0.82f, 1.0f, 0.86f, titleScreen.centered);
    }

    bool promptVisible = !titleScreen.blinkPrompt || ((GetTickCount64() / 520) % 2 == 0);
    if (promptVisible) {
        AppendTitleScreenOverlay(overlays, L"START " + titleScreen.start + L" -> " + titleScreen.next, centerX, legendY + smallScale * 9.5f, smallScale, 0.74f, 0.92f, 0.72f, 0.94f, titleScreen.centered);
    }

    if (titleScreen.showCredit) {
        AppendTitleScreenOverlay(overlays, titleScreen.credit, centerX, viewport.playTop + viewport.playHeight - smallScale * 10.0f, smallScale, 0.48f, 0.62f, 0.78f, 0.9f, titleScreen.centered);
    }

    return overlays;
}

void ApplySceneOverlayTextToRenderer(const std::vector<BgeSceneOverlayText>& overlays)
{
    if (g_directX11Renderer) {
        g_directX11Renderer->SetSceneOverlayText(overlays);
    }
    if (g_directX12Renderer) {
        g_directX12Renderer->SetSceneOverlayText(overlays);
    }
}

bool ExecuteBgeTitleScreenCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"title-screen commands run in bge.game-loop";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
    if (subcommand == L"status") {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        statusText = TitleScreenStatusText(g_titleScreenState);
        return true;
    }

    if (subcommand == L"hide" || subcommand == L"clear") {
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            g_titleScreenState.visible = false;
            g_titleScreenActive = false;
            g_rendererStateDirty = true;
        }
        SetGameHudStatus(L"Title screen hidden");
        InvalidateGameRenderer();
        statusText = L"Title screen hidden";
        return true;
    }

    if (subcommand != L"create" && subcommand != L"show") {
        statusText = L"Use: title-screen create --text ASTEROIDS --subtitle PRESS_ENTER --start Enter --next playing";
        return false;
    }

    if (!BgePluginAlreadyImported(L"bge.piece.title-screen")) {
        statusText = L"Import title-screen first: plugin import bge.piece.title-screen";
        return false;
    }

    BgeTitleScreenState titleScreen;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        titleScreen = g_titleScreenState;
    }
    titleScreen.visible = true;
    titleScreen.centered = true;

    std::wstring value;
    if (TryGetCommandOptionValue(tokens, L"--text", value)) {
        titleScreen.text = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--subtitle", value)) {
        titleScreen.subtitle = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--start", value)) {
        titleScreen.start = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--next", value)) {
        titleScreen.next = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--font", value)) {
        titleScreen.font = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--legend", value)) {
        titleScreen.legend = NormalizeTitleScreenText(value);
        titleScreen.showLegend = !titleScreen.legend.empty();
    }
    if (TryGetCommandOptionValue(tokens, L"--credit", value)) {
        titleScreen.credit = NormalizeTitleScreenText(value);
        titleScreen.showCredit = !titleScreen.credit.empty();
    }
    if (HasCommandFlag(tokens, L"--left")) {
        titleScreen.centered = false;
    }
    if (HasCommandFlag(tokens, L"--center")) {
        titleScreen.centered = true;
    }
    if (HasCommandFlag(tokens, L"--no-legend")) {
        titleScreen.showLegend = false;
    }
    if (HasCommandFlag(tokens, L"--no-credit")) {
        titleScreen.showCredit = false;
    }
    if (HasCommandFlag(tokens, L"--blink")) {
        titleScreen.blinkPrompt = true;
    }
    if (HasCommandFlag(tokens, L"--no-blink")) {
        titleScreen.blinkPrompt = false;
    }

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        g_titleScreenState = titleScreen;
        g_titleScreenActive = true;
        g_rendererStateDirty = true;
    }

    SetGameHudStatus(TitleScreenStatusText(titleScreen));
    InvalidateGameRenderer();
    statusText = TitleScreenStatusText(titleScreen);
    LogRendererMessage("[BgeTitleScreenPlugin] screen-defined text=\"" + Narrow(titleScreen.text) + "\" subtitle=\"" + Narrow(titleScreen.subtitle) + "\"");
    return true;
}

bool TryParseIntArg(const std::wstring& text, int& value)
{
    wchar_t* parseEnd = nullptr;
    long parsed = std::wcstol(text.c_str(), &parseEnd, 10);
    if (parseEnd == text.c_str() || *parseEnd != L'\0') {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

std::vector<std::wstring> SplitBgeListText(std::wstring text)
{
    std::replace(text.begin(), text.end(), L',', L'|');
    std::vector<std::wstring> values;
    std::wstringstream stream(text);
    std::wstring item;
    while (std::getline(stream, item, L'|')) {
        item = TrimText(item);
        if (!item.empty()) {
            values.push_back(item);
        }
    }
    return values;
}

BgeCounterState* FindBgeCounterMutable(const std::wstring& name)
{
    std::wstring requested = LowerArg(name);
    for (auto& counter : g_bgeCounters) {
        if (LowerArg(counter.name) == requested) {
            return &counter;
        }
    }
    return nullptr;
}

const BgeCounterState* FindBgeCounter(const std::vector<BgeCounterState>& counters, const std::wstring& name)
{
    std::wstring requested = LowerArg(name);
    for (const auto& counter : counters) {
        if (LowerArg(counter.name) == requested) {
            return &counter;
        }
    }
    return nullptr;
}

int ClampCounterValue(const BgeCounterState& counter, int value)
{
    if (counter.hasMin) {
        value = (std::max)(counter.minValue, value);
    }
    if (counter.hasMax) {
        value = (std::min)(counter.maxValue, value);
    }
    return value;
}

void EnsureBgeCounterLocked(const std::wstring& name)
{
    if (name.empty() || FindBgeCounterMutable(name)) {
        return;
    }
    BgeCounterState counter;
    counter.name = name;
    g_bgeCounters.push_back(counter);
}

bool AddBgeCounterValueLocked(const std::wstring& name, int delta)
{
    if (name.empty()) {
        return false;
    }
    EnsureBgeCounterLocked(name);
    BgeCounterState* counter = FindBgeCounterMutable(name);
    if (!counter) {
        return false;
    }
    counter->value = ClampCounterValue(*counter, counter->value + delta);
    return true;
}

std::wstring BgeCountersStatusText(const std::vector<BgeCounterState>& counters)
{
    if (counters.empty()) {
        return L"Counters: none";
    }

    std::wstring text = L"Counters:";
    for (const auto& counter : counters) {
        text += L" " + counter.name + L"=" + std::to_wstring(counter.value);
    }
    return text;
}

std::wstring ReplaceAllText(std::wstring text, const std::wstring& from, const std::wstring& to)
{
    if (from.empty()) {
        return text;
    }
    size_t offset = 0;
    while ((offset = text.find(from, offset)) != std::wstring::npos) {
        text.replace(offset, from.size(), to);
        offset += to.size();
    }
    return text;
}

std::wstring FormatScoreboardOverlayText(const BgeScoreboardState& scoreboard, const std::vector<BgeCounterState>& counters)
{
    std::wstring formatted = scoreboard.format.empty() ? L"SCORE:{score}|HIGH:{high-score}|LIVES:{lives}|WAVE:{wave}" : scoreboard.format;
    for (const auto& counter : counters) {
        formatted = ReplaceAllText(formatted, L"{" + counter.name + L"}", std::to_wstring(counter.value));
    }
    return NormalizeTitleScreenText(formatted);
}

std::vector<std::wstring> FormatScoreboardOverlayRows(const BgeScoreboardState& scoreboard, const std::vector<BgeCounterState>& counters)
{
    std::wstring formatted = scoreboard.format.empty() ? L"SCORE:{score}|HIGH:{high-score}|LIVES:{lives}|WAVE:{wave}" : scoreboard.format;
    for (const auto& counter : counters) {
        formatted = ReplaceAllText(formatted, L"{" + counter.name + L"}", std::to_wstring(counter.value));
    }

    std::vector<std::wstring> rows = SplitBgeListText(formatted);
    if (rows.empty()) {
        rows.push_back(formatted);
    }
    for (std::wstring& row : rows) {
        row = NormalizeTitleScreenText(row);
    }
    return rows;
}

std::wstring ScoreboardStatusText(const BgeScoreboardState& scoreboard, const std::vector<BgeCounterState>& counters)
{
    if (!scoreboard.visible) {
        return L"Scoreboard hidden";
    }
    return L"Scoreboard: " + FormatScoreboardOverlayText(scoreboard, counters);
}

std::vector<BgeSceneOverlayText> BuildScoreboardOverlays(const BgeScoreboardState& scoreboard, const std::vector<BgeCounterState>& counters, const BgeGameViewport& viewport)
{
    std::vector<BgeSceneOverlayText> overlays;
    if (!scoreboard.visible || viewport.width <= 1.0f || viewport.playHeight <= 1.0f) {
        return overlays;
    }

    std::wstring anchor = LowerArg(scoreboard.anchor);
    bool centered = anchor == L"top-center" || anchor == L"center-top";
    float scale = ClampFloat(scoreboard.scale, 1.2f, g_playerRuntimeMode ? 5.0f : 4.0f);
    float x = 18.0f;
    float y = viewport.playTop + 12.0f;
    if (centered) {
        x = viewport.width * 0.5f;
    }
    else if (anchor == L"top-right" || anchor == L"right") {
        x = (std::max)(18.0f, viewport.width - 380.0f);
    }

    std::vector<std::wstring> rows = FormatScoreboardOverlayRows(scoreboard, counters);
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const std::wstring& row = rows[rowIndex];
        bool rightAlignedWave = !centered && LowerArg(row).rfind(L"wave", 0) == 0;
        float rowX = rightAlignedWave ? (std::max)(18.0f, viewport.width - 210.0f) : x;
        float rowY = rightAlignedWave ? y : y + static_cast<float>(rowIndex) * scale * 8.8f;
        AppendTitleScreenOverlay(overlays, row, rowX, rowY, scale, 0.82f, 0.92f, 1.0f, 0.96f, centered);
    }
    if (!scoreboard.iconRow.empty()) {
        AppendTitleScreenOverlay(overlays, NormalizeTitleScreenText(scoreboard.iconRow), x, y + static_cast<float>(rows.size()) * scale * 8.8f, ClampFloat(scale * 0.7f, 1.0f, 2.4f), 0.55f, 0.72f, 0.88f, 0.82f, centered);
    }
    return overlays;
}

bool ExecuteBgeCounterCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"counter commands run in bge.game-loop";
        return false;
    }
    if (!BgePluginAlreadyImported(L"bge.2d.arcade")) {
        statusText = L"Import arcade counters first: plugin import bge.2d.arcade";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
    if (subcommand == L"status" || subcommand == L"list") {
        std::vector<BgeCounterState> counters;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            counters = g_bgeCounters;
        }
        statusText = BgeCountersStatusText(counters);
        return true;
    }

    if (tokens.size() < 4 || (subcommand != L"define" && subcommand != L"set" && subcommand != L"add" && subcommand != L"increment")) {
        statusText = L"Use: counter define <name> <value> [min n] [max n] [persist session] | counter set <name> <value> | counter add <name> <delta>";
        return false;
    }

    int value = 0;
    if (!TryParseIntArg(tokens[3], value)) {
        statusText = L"Counter value must be an integer";
        return false;
    }

    std::wstring counterName = tokens[2];
    std::wstring action;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        EnsureBgeCounterLocked(counterName);
        BgeCounterState* counter = FindBgeCounterMutable(counterName);
        if (!counter) {
            statusText = L"Counter unavailable";
            return false;
        }

        if (subcommand == L"define") {
            counter->value = value;
            for (size_t index = 4; index + 1 < tokens.size(); ++index) {
                std::wstring key = LowerArg(tokens[index]);
                int parsed = 0;
                if (key == L"min" && TryParseIntArg(tokens[index + 1], parsed)) {
                    counter->minValue = parsed;
                    counter->hasMin = true;
                    ++index;
                }
                else if (key == L"max" && TryParseIntArg(tokens[index + 1], parsed)) {
                    counter->maxValue = parsed;
                    counter->hasMax = true;
                    ++index;
                }
                else if (key == L"persist" && LowerArg(tokens[index + 1]) == L"session") {
                    counter->persistSession = true;
                    ++index;
                }
            }
            action = L"defined";
        }
        else if (subcommand == L"set") {
            counter->value = value;
            action = L"set";
        }
        else {
            counter->value += value;
            action = L"changed";
        }
        counter->value = ClampCounterValue(*counter, counter->value);
        g_rendererStateDirty = true;
        statusText = L"Counter " + action + L": " + counter->name + L"=" + std::to_wstring(counter->value);
    }

    InvalidateGameRenderer();
    LogRendererMessage("[BgeCounter] " + Narrow(action) + " name=\"" + Narrow(counterName) + "\" value=" + Narrow(std::to_wstring(value)));
    return true;
}

bool ExecuteBgeScoreboardCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"scoreboard commands run in bge.game-loop";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
    if (subcommand == L"status") {
        BgeScoreboardState scoreboard;
        std::vector<BgeCounterState> counters;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            scoreboard = g_scoreboardState;
            counters = g_bgeCounters;
        }
        statusText = ScoreboardStatusText(scoreboard, counters);
        return true;
    }

    if (subcommand == L"hide" || subcommand == L"clear") {
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            g_scoreboardState.visible = false;
            g_scoreboardActive = false;
            g_rendererStateDirty = true;
        }
        InvalidateGameRenderer();
        statusText = L"Scoreboard hidden";
        return true;
    }

    if (subcommand != L"create" && subcommand != L"show") {
        statusText = L"Use: scoreboard create --counters score|high-score|lives|wave --anchor top-left --format SCORE:{score}|HIGH:{high-score}|LIVES:{lives}|WAVE:{wave}";
        return false;
    }
    if (!BgePluginAlreadyImported(L"bge.piece.scoreboard")) {
        statusText = L"Import scoreboard first: plugin import bge.piece.scoreboard";
        return false;
    }

    BgeScoreboardState scoreboard;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        scoreboard = g_scoreboardState;
    }
    scoreboard.visible = true;

    std::wstring value;
    if (TryGetCommandOptionValue(tokens, L"--counters", value)) {
        scoreboard.counters = SplitBgeListText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--anchor", value)) {
        scoreboard.anchor = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--format", value)) {
        scoreboard.format = value;
    }
    if (TryGetCommandOptionValue(tokens, L"--font", value)) {
        scoreboard.font = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--color", value)) {
        scoreboard.color = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--icon-row", value)) {
        scoreboard.iconRow = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--scale", value)) {
        float parsedScale = 0.0f;
        if (TryParseFloatArg(value, parsedScale)) {
            scoreboard.scale = parsedScale;
        }
    }

    std::vector<BgeCounterState> counters;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        for (const auto& counterName : scoreboard.counters) {
            EnsureBgeCounterLocked(counterName);
        }
        g_scoreboardState = scoreboard;
        g_scoreboardActive = true;
        g_rendererStateDirty = true;
        counters = g_bgeCounters;
    }

    InvalidateGameRenderer();
    statusText = ScoreboardStatusText(scoreboard, counters);
    LogRendererMessage("[BgeScoreboardPlugin] overlay-defined text=\"" + Narrow(FormatScoreboardOverlayText(scoreboard, counters)) + "\"");
    return true;
}

std::wstring JoinBgeListText(const std::vector<std::wstring>& values)
{
    std::wstring text;
    for (size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            text += L"|";
        }
        text += values[index];
    }
    return text;
}

float BgeRockRadiusForSize(const std::wstring& size)
{
    std::wstring lower = LowerArg(size);
    if (lower == L"small") {
        return 22.0f;
    }
    if (lower == L"medium") {
        return 38.0f;
    }
    return 58.0f;
}

int BgeRockSizeIndexForRadius(const BgeBreakableRockFieldState& field, float radius)
{
    if (field.sizes.empty()) {
        return 0;
    }

    int bestIndex = 0;
    float bestDistance = std::fabs(radius - BgeRockRadiusForSize(field.sizes.front()));
    for (int index = 1; index < static_cast<int>(field.sizes.size()); ++index) {
        float distance = std::fabs(radius - BgeRockRadiusForSize(field.sizes[index]));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }
    return bestIndex;
}

bool TryParseBgeRockScoreRule(const BgeBreakableRockFieldState& field, std::wstring& counterName, std::vector<int>& scoreValues)
{
    counterName = L"score";
    scoreValues.clear();

    std::wstring scoreText = TrimText(field.score);
    size_t separator = scoreText.find(L':');
    if (separator != std::wstring::npos) {
        counterName = TrimText(scoreText.substr(0, separator));
        scoreText = scoreText.substr(separator + 1);
    }

    for (const std::wstring& part : SplitBgeListText(scoreText)) {
        int value = 0;
        if (TryParseIntArg(part, value)) {
            scoreValues.push_back(value);
        }
    }
    return !counterName.empty() && !scoreValues.empty();
}

int BgeRockScoreForSizeIndex(const BgeBreakableRockFieldState& field, int sizeIndex, std::wstring& counterName)
{
    std::vector<int> scoreValues;
    if (!TryParseBgeRockScoreRule(field, counterName, scoreValues)) {
        return 0;
    }
    int clampedIndex = (std::max)(0, (std::min)(sizeIndex, static_cast<int>(scoreValues.size()) - 1));
    return scoreValues[clampedIndex];
}

bool TryParseBgeRange(const std::wstring& text, float& minimumValue, float& maximumValue)
{
    size_t separator = text.find(L"..");
    if (separator == std::wstring::npos) {
        return false;
    }
    float first = 0.0f;
    float second = 0.0f;
    if (!TryParseFloatArg(text.substr(0, separator), first) || !TryParseFloatArg(text.substr(separator + 2), second)) {
        return false;
    }
    minimumValue = (std::min)(first, second);
    maximumValue = (std::max)(first, second);
    return true;
}

bool TryParseBgeAvoidSpec(const std::wstring& text, std::wstring& avoidId, float& avoidRadius)
{
    size_t separator = text.find(L':');
    if (separator == std::wstring::npos) {
        avoidId = NormalizeTitleScreenText(text);
        return !avoidId.empty();
    }

    float parsedRadius = 0.0f;
    if (!TryParseFloatArg(text.substr(separator + 1), parsedRadius)) {
        return false;
    }
    avoidId = NormalizeTitleScreenText(text.substr(0, separator));
    avoidRadius = ClampFloat(parsedRadius, 0.0f, 1000.0f);
    return !avoidId.empty();
}

std::wstring BreakableRockFieldStatusText(const BgeBreakableRockFieldState& field)
{
    if (!field.active) {
        return L"Rock field hidden";
    }
    return L"Rock field: " + std::to_wstring(field.count)
        + L" rocks sizes=" + JoinBgeListText(field.sizes)
        + L" split=" + std::to_wstring(field.splitCount)
        + L" score=" + field.score
        + L" speed=" + std::to_wstring(static_cast<int>(field.speedMin)) + L".." + std::to_wstring(static_cast<int>(field.speedMax))
        + L" wave=" + field.waveCounter
        + L" avoid=" + field.avoid + L":" + std::to_wstring(static_cast<int>(field.avoidRadius));
}

int FindReusableBgeRockSlotLocked(int reservedProjectileSlots = 1)
{
    int availableFreeSlots = 0;
    int candidateSlot = -1;
    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        if (g_mainPlayerGroupIndex == g_activeObjectGroupIndex && index == g_mainPlayerSlot) {
            continue;
        }
        if (!g_objectSlots[index].visible || g_objectSlots[index].isDeleted) {
            ++availableFreeSlots;
            if (candidateSlot < 0) {
                candidateSlot = index;
            }
        }
    }
    return availableFreeSlots > reservedProjectileSlots ? candidateSlot : -1;
}

void ConfigureBgeSplitRockSlotLocked(int slotIndex, const BgeBreakableRockFieldState& field, const BgeObjectSlotState& sourceRock, const std::wstring& size, int splitIndex, int splitTotal)
{
    float sourceSpeed = std::sqrt(sourceRock.velocityX * sourceRock.velocityX + sourceRock.velocityY * sourceRock.velocityY);
    float baseSpeed = ClampFloat((std::max)(sourceSpeed, field.speedMin) * 1.08f, field.speedMin, field.speedMax + 80.0f);
    float sourceAngle = std::atan2(sourceRock.velocityY, sourceRock.velocityX);
    float spread = splitTotal <= 1 ? 0.0f : (static_cast<float>(splitIndex) - (static_cast<float>(splitTotal - 1) * 0.5f)) * 0.72f;
    float angle = sourceAngle + spread + 0.38f;

    BgeObjectSlotState& slot = g_objectSlots[slotIndex];
    slot = sourceRock;
    slot.visible = true;
    slot.deleteMarked = false;
    slot.isDeleted = false;
    slot.collisionDetected = false;
    slot.radius = BgeRockRadiusForSize(size);
    slot.x = sourceRock.x + std::cos(angle) * (slot.radius + 6.0f);
    slot.y = sourceRock.y + std::sin(angle) * (slot.radius + 6.0f);
    slot.velocityX = std::cos(angle) * baseSpeed;
    slot.velocityY = std::sin(angle) * baseSpeed;
    slot.colorA = 0.90f;
    slot.shape = BgeObjectShape::Asteroid;
    slot.kind = BgeObjectKind::Asteroid;
}

void ConfigureBgeRockSlotLocked(int slotIndex, const BgeBreakableRockFieldState& field, int rockIndex, const BgeGameViewport& viewport)
{
    float radius = field.sizes.empty() ? 58.0f : BgeRockRadiusForSize(field.sizes.front());
    float width = (std::max)(viewport.width, radius * 4.0f);
    float height = (std::max)(viewport.playHeight, radius * 4.0f);
    float phase = static_cast<float>(rockIndex) * 2.39996323f;
    float unitX = 0.5f + std::cos(phase) * 0.34f;
    float unitY = 0.5f + std::sin(phase) * 0.34f;
    float x = ClampFloat(width * unitX, radius, (std::max)(radius, width - radius));
    float y = ClampFloat(viewport.playTop + height * unitY, viewport.playTop + radius, (std::max)(viewport.playTop + radius, viewport.playTop + height - radius));

    if (VectorShipPlayerAliveLocked()) {
        const BgeObjectSlotState& player = g_objectSlots[g_vectorShipState.slotIndex];
        float dx = x - player.x;
        float dy = y - player.y;
        float avoidDistance = field.avoidRadius + radius;
        if (dx * dx + dy * dy < avoidDistance * avoidDistance) {
            x = width - x;
            y = viewport.playTop + height - (y - viewport.playTop);
        }
    }

    float speedSpan = (std::max)(0.0f, field.speedMax - field.speedMin);
    float speed = field.speedMin + speedSpan * (static_cast<float>((rockIndex * 37) % 100) / 99.0f);
    float angle = 0.73f + static_cast<float>(rockIndex) * 1.41f;

    BgeObjectSlotState& slot = g_objectSlots[slotIndex];
    slot = BgeObjectSlotState{};
    slot.visible = true;
    slot.x = x;
    slot.y = y;
    slot.radius = radius;
    slot.velocityX = std::cos(angle) * speed;
    slot.velocityY = std::sin(angle) * speed;
    slot.colorR = 0.74f;
    slot.colorG = 0.78f;
    slot.colorB = 0.82f;
    slot.colorA = 0.90f;
    slot.shape = BgeObjectShape::Asteroid;
    slot.kind = BgeObjectKind::Asteroid;
    slot.renderStyle = field.renderStyle;
    slot.outlineThickness = field.outlineThickness;
}

bool ExecuteBgeBreakableRockFieldCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"rock-field commands run in bge.game-loop";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
    if (subcommand == L"status") {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        statusText = BreakableRockFieldStatusText(g_breakableRockFieldState);
        return true;
    }

    if (subcommand == L"hide" || subcommand == L"clear") {
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            for (BgeObjectSlotState& slot : g_objectSlots) {
                if (slot.kind == BgeObjectKind::Asteroid) {
                    slot.visible = false;
                    slot.kind = BgeObjectKind::Generic;
                }
            }
            g_breakableRockFieldState.active = false;
            g_breakableRockFieldActive = false;
            g_rendererStateDirty = true;
            PersistActiveObjectGroupLocked();
        }
        InvalidateGameRenderer();
        statusText = L"Rock field hidden";
        return true;
    }

    if (subcommand != L"create" && subcommand != L"show") {
        statusText = L"Use: rock-field create --sizes large|medium|small --count 4 --split 2 --score score:20|50|100 --speed-range 30..180 --wave-counter wave --avoid player_ship:160";
        return false;
    }
    if (!BgePluginAlreadyImported(L"bge.piece.breakable-rock-field")) {
        statusText = L"Import breakable rock field first: plugin import bge.piece.breakable-rock-field";
        return false;
    }

    BgeBreakableRockFieldState field;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        field = g_breakableRockFieldState;
    }
    field.active = true;

    std::wstring value;
    if (TryGetCommandOptionValue(tokens, L"--sizes", value)) {
        std::vector<std::wstring> sizes = SplitBgeListText(value);
        if (!sizes.empty()) {
            field.sizes = sizes;
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--count", value)) {
        int parsedCount = 0;
        if (TryParseIntArg(value, parsedCount)) {
            field.count = (std::max)(0, (std::min)(parsedCount, BGE_OBJECT_SLOT_COUNT - 1));
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--split", value)) {
        int parsedSplit = 0;
        if (TryParseIntArg(value, parsedSplit)) {
            field.splitCount = (std::max)(0, (std::min)(parsedSplit, 8));
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--score", value)) {
        field.score = TrimText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--speed-range", value)) {
        float minimumSpeed = 0.0f;
        float maximumSpeed = 0.0f;
        if (TryParseBgeRange(value, minimumSpeed, maximumSpeed)) {
            field.speedMin = ClampFloat(minimumSpeed, 0.0f, 1000.0f);
            field.speedMax = ClampFloat(maximumSpeed, field.speedMin, 1200.0f);
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--wave-counter", value)) {
        field.waveCounter = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--avoid", value)) {
        TryParseBgeAvoidSpec(value, field.avoid, field.avoidRadius);
    }
    // Recipe-driven render style for the spawned rocks; see the
    // matching block on player-ship create.
    if (TryGetCommandOptionValue(tokens, L"--style", value)) {
        BgeObjectRenderStyle parsedStyle = BgeObjectRenderStyle::Filled;
        if (BgeTryParseObjectRenderStyle(LowerArg(value), parsedStyle)) {
            field.renderStyle = parsedStyle;
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--outline-thickness", value)) {
        float parsedThickness = 0.0f;
        if (TryParseFloatArg(value, parsedThickness)) {
            field.outlineThickness = ClampFloat(parsedThickness, 0.5f, 16.0f);
        }
    }

    int spawned = 0;
    BgeGameViewport viewport = CurrentGameViewport();
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        for (BgeObjectSlotState& slot : g_objectSlots) {
            if (slot.kind == BgeObjectKind::Asteroid) {
                slot.visible = false;
                slot.kind = BgeObjectKind::Generic;
            }
        }
        for (int rockIndex = 0; rockIndex < field.count; ++rockIndex) {
            int slotIndex = FindReusableBgeRockSlotLocked();
            if (slotIndex < 0) {
                break;
            }
            ConfigureBgeRockSlotLocked(slotIndex, field, rockIndex, viewport);
            ++spawned;
        }
        field.count = spawned;
        g_breakableRockFieldState = field;
        g_breakableRockFieldActive = spawned > 0;
        g_ballAnimationRunning = true;
        g_rendererStateDirty = true;
        BgeSetCurrentEdgePolicy(BgeEdgePolicy::Wrap);
        BgeUpdateCollisionFlags(g_objectSlots);
        PersistActiveObjectGroupLocked();
    }

    InvalidateGameRenderer();
    statusText = BreakableRockFieldStatusText(field);
    LogRendererMessage("[BgeBreakableRockFieldPlugin] rock-field-spawned count=" + std::to_string(spawned) + " sizes=\"" + Narrow(JoinBgeListText(field.sizes)) + "\"");
    return spawned > 0;
}

BgeProjectileDefinition* FindBgeProjectileDefinitionMutable(const std::wstring& id)
{
    std::wstring requested = LowerArg(id);
    for (auto& definition : g_projectileDefinitions) {
        if (LowerArg(definition.id) == requested) {
            return &definition;
        }
    }
    return nullptr;
}

const BgeProjectileDefinition* FindBgeProjectileDefinition(const std::wstring& id)
{
    std::wstring requested = LowerArg(id);
    for (const auto& definition : g_projectileDefinitions) {
        if (LowerArg(definition.id) == requested) {
            return &definition;
        }
    }
    return nullptr;
}

std::wstring ProjectileDefinitionStatusText(const std::vector<BgeProjectileDefinition>& definitions)
{
    if (definitions.empty()) {
        return L"Projectiles: none";
    }

    std::wstring text = L"Projectiles:";
    for (const auto& definition : definitions) {
        text += L" " + definition.id
            + L" owner=" + definition.owner
            + L" shape=" + BgeObjectShapeName(definition.shape)
            + L" speed=" + std::to_wstring(static_cast<int>(definition.speed))
            + L" ttl=" + std::to_wstring(definition.ttlSeconds);
    }
    return text;
}

int FindReusableBgeProjectileSlotLocked()
{
    for (int index = BGE_OBJECT_SLOT_COUNT - 1; index >= 0; --index) {
        if (g_mainPlayerGroupIndex == g_activeObjectGroupIndex && index == g_mainPlayerSlot) {
            continue;
        }
        if (!g_objectSlots[index].visible || g_objectSlots[index].isDeleted || (g_objectSlots[index].kind == BgeObjectKind::Bullet && g_bgeProjectileLifeSeconds[index] <= 0.0f)) {
            return index;
        }
    }

    int oldestProjectileSlot = -1;
    float shortestLife = 1000000.0f;
    for (int index = BGE_OBJECT_SLOT_COUNT - 1; index >= 0; --index) {
        if (g_mainPlayerGroupIndex == g_activeObjectGroupIndex && index == g_mainPlayerSlot) {
            continue;
        }
        if (g_objectSlots[index].kind == BgeObjectKind::Bullet && g_bgeProjectileLifeSeconds[index] < shortestLife) {
            shortestLife = g_bgeProjectileLifeSeconds[index];
            oldestProjectileSlot = index;
        }
    }
    return oldestProjectileSlot;
}

bool SpawnBgeProjectileLocked(const BgeProjectileDefinition& definition, const BgeObjectSlotState& source, int& projectileSlot)
{
    projectileSlot = FindReusableBgeProjectileSlotLocked();
    if (projectileSlot < 0) {
        return false;
    }

    // Fire along the ship's authored heading so projectiles follow the nose
    // even when the ship is drifting sideways or stationary. Fall back to
    // velocity (legacy behavior) for sources that never set a heading.
    float directionX = source.headingX;
    float directionY = source.headingY;
    float headingLength = std::sqrt(directionX * directionX + directionY * directionY);
    if (headingLength < 0.001f) {
        directionX = source.velocityX;
        directionY = source.velocityY;
        headingLength = std::sqrt(directionX * directionX + directionY * directionY);
    }
    if (headingLength < 1.0f) {
        directionX = 1.0f;
        directionY = 0.0f;
        headingLength = 1.0f;
    }
    directionX /= headingLength;
    directionY /= headingLength;

    BgeObjectSlotState& projectile = g_objectSlots[projectileSlot];
    projectile = BgeObjectSlotState{};
    projectile.visible = true;
    projectile.x = source.x + directionX * (source.radius + 14.0f);
    projectile.y = source.y + directionY * (source.radius + 14.0f);
    projectile.radius = definition.shape == BgeObjectShape::Line ? 4.0f : 5.0f;
    // Asteroids bullets fly at a fixed speed in the firing direction; they do
    // NOT inherit the ship's drift. This keeps shots predictable.
    projectile.velocityX = directionX * definition.speed;
    projectile.velocityY = directionY * definition.speed;
    projectile.headingX = directionX;
    projectile.headingY = directionY;
    projectile.colorR = 1.0f;
    projectile.colorG = 0.92f;
    projectile.colorB = 0.18f;
    projectile.colorA = 1.0f;
    projectile.shape = definition.shape;
    projectile.kind = BgeObjectKind::Bullet;
    g_bgeProjectileLifeSeconds[projectileSlot] = (std::max)(0.05f, definition.ttlSeconds);
    if (definition.wrap) {
        BgeSetCurrentEdgePolicy(BgeEdgePolicy::Wrap);
    }
    return true;
}

bool FireBgeProjectileFromVectorShipLocked(std::wstring& statusText)
{
    if (!BgePluginAlreadyImported(L"bge.piece.projectile")) {
        statusText = L"Player ship: import projectile first: plugin import bge.piece.projectile";
        return false;
    }
    if (g_vectorShipState.fire.empty()) {
        statusText = L"Player ship: fire unbound";
        return false;
    }

    const BgeProjectileDefinition* definition = FindBgeProjectileDefinition(g_vectorShipState.fire);
    if (!definition) {
        statusText = L"Player ship: projectile " + g_vectorShipState.fire + L" not defined";
        return false;
    }
    if (!VectorShipPlayerAliveLocked()) {
        statusText = L"Player ship unavailable";
        return false;
    }

    int projectileSlot = -1;
    // Freshen the ship slot's transformed heading before firing so bullets,
    // thrust, and the rendered nose all use the same 0-9 trial mode.
    {
        BgeObjectSlotState& shipSlot = g_objectSlots[g_vectorShipState.slotIndex];
        ApplyVectorShipHeadingModeLocked(shipSlot, g_vectorShipState);
    }
    if (!SpawnBgeProjectileLocked(*definition, g_objectSlots[g_vectorShipState.slotIndex], projectileSlot)) {
        statusText = L"Player ship: no projectile slot";
        return false;
    }

    statusText = L"Player ship: fire " + definition->id + L" object " + std::to_wstring(projectileSlot + 1);
    return true;
}

float RandomBgeUnitFloat()
{
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

float RandomBgeRange(float minimum, float maximum)
{
    if (maximum <= minimum) {
        return minimum;
    }
    return minimum + (maximum - minimum) * RandomBgeUnitFloat();
}

void ResetBgeUfoArrivalTimerLocked(float minimumOverride = -1.0f, float maximumOverride = -1.0f)
{
    float minimum = minimumOverride >= 0.0f ? minimumOverride : g_ufoState.arrivalMinSeconds;
    float maximum = maximumOverride >= 0.0f ? maximumOverride : g_ufoState.arrivalMaxSeconds;
    if (maximum < minimum) {
        maximum = minimum;
    }
    g_ufoState.arrivalRemainingSeconds = RandomBgeRange(minimum, maximum);
}

bool BgeUfoScoreSpec(const std::wstring& scoreSpec, std::wstring& counterName, int& points)
{
    std::wstring trimmed = TrimText(scoreSpec);
    if (trimmed.empty()) {
        counterName = L"score";
        points = 200;
        return true;
    }

    size_t colon = trimmed.find(L':');
    std::wstring pointsText;
    if (colon == std::wstring::npos) {
        counterName = L"score";
        pointsText = trimmed;
    }
    else {
        counterName = NormalizeTitleScreenText(trimmed.substr(0, colon));
        pointsText = trimmed.substr(colon + 1);
    }
    if (counterName.empty()) {
        counterName = L"score";
    }

    int parsedPoints = 0;
    if (!TryParseIntArg(pointsText, parsedPoints)) {
        return false;
    }
    points = parsedPoints;
    return true;
}

std::wstring BgeUfoStatusText(const BgeUfoState& ufo)
{
    return L"UFO: " + std::wstring(ufo.active ? L"active" : L"inactive")
        + L" id=" + ufo.id
        + L" score=" + ufo.score
        + L" weapon=" + ufo.weapon
        + L" arrival=" + std::to_wstring(static_cast<int>(ufo.arrivalMinSeconds)) + L".." + std::to_wstring(static_cast<int>(ufo.arrivalMaxSeconds))
        + L" next=" + std::to_wstring(static_cast<int>(ufo.arrivalRemainingSeconds));
}

void HideBgeUfoSlotLocked(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
        return;
    }
    BgeObjectSlotState& slot = g_objectSlots[slotIndex];
    if (slot.kind != BgeObjectKind::Ufo) {
        return;
    }
    slot.visible = false;
    slot.deleteMarked = false;
    slot.isDeleted = false;
    slot.collisionDetected = false;
    slot.kind = BgeObjectKind::Generic;
    if (g_ufoState.slotIndex == slotIndex) {
        g_ufoState.slotIndex = -1;
    }
}

void HideBgeUfoLocked()
{
    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        HideBgeUfoSlotLocked(index);
    }
}

int ActiveBgeUfoSlotLocked()
{
    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        const BgeObjectSlotState& slot = g_objectSlots[index];
        if (slot.visible && !slot.isDeleted && slot.kind == BgeObjectKind::Ufo) {
            return index;
        }
    }
    return -1;
}

int FindReusableBgeUfoSlotLocked()
{
    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        if (g_mainPlayerGroupIndex == g_activeObjectGroupIndex && index == g_mainPlayerSlot) {
            continue;
        }
        if (g_objectSlots[index].kind == BgeObjectKind::Bullet && g_bgeProjectileLifeSeconds[index] > 0.0f) {
            continue;
        }
        if (!g_objectSlots[index].visible || g_objectSlots[index].isDeleted) {
            return index;
        }
    }
    return -1;
}

bool SpawnBgeUfoLocked()
{
    int slotIndex = FindReusableBgeUfoSlotLocked();
    if (slotIndex < 0) {
        return false;
    }

    BgeGameViewport viewport = CurrentGameViewport();
    float radius = (std::max)(10.0f, g_ufoState.radius);
    float minY = viewport.playTop + radius * 1.8f;
    float maxY = (std::max)(minY, viewport.playTop + viewport.playHeight - radius * 1.8f);
    bool fromLeft = RandomBgeUnitFloat() < 0.5f;
    float direction = fromLeft ? 1.0f : -1.0f;
    float speed = (std::max)(40.0f, g_ufoState.speed);

    BgeObjectSlotState& slot = g_objectSlots[slotIndex];
    slot = BgeObjectSlotState{};
    slot.visible = true;
    slot.x = fromLeft ? -radius * 2.5f : viewport.width + radius * 2.5f;
    slot.y = RandomBgeRange(minY, maxY);
    slot.radius = radius;
    slot.velocityX = direction * speed;
    slot.velocityY = RandomBgeRange(-38.0f, 38.0f);
    slot.headingX = direction;
    slot.headingY = 0.0f;
    slot.colorR = 0.84f;
    slot.colorG = 0.96f;
    slot.colorB = 1.0f;
    slot.colorA = 1.0f;
    slot.shape = BgeObjectShape::Ufo;
    slot.kind = BgeObjectKind::Ufo;
    slot.renderStyle = g_ufoState.renderStyle;
    slot.outlineThickness = g_ufoState.outlineThickness;
    g_ufoState.slotIndex = slotIndex;
    return true;
}

bool AwardBgeUfoScoreLocked(int& scoreDelta)
{
    std::wstring counterName;
    int points = 0;
    if (!BgeUfoScoreSpec(g_ufoState.score, counterName, points)) {
        return false;
    }
    if (points != 0 && AddBgeCounterValueLocked(counterName, points)) {
        scoreDelta += points;
        return true;
    }
    return false;
}

bool ExecuteBgeUfoCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"ufo commands run in bge.game-loop";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
    if (subcommand == L"status" || subcommand == L"list") {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        statusText = BgeUfoStatusText(g_ufoState);
        return true;
    }

    if (subcommand == L"hide" || subcommand == L"clear") {
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            HideBgeUfoLocked();
            g_ufoState.active = false;
            g_ufoActive = false;
            g_rendererStateDirty = true;
            PersistActiveObjectGroupLocked();
        }
        InvalidateGameRenderer();
        statusText = L"UFO hidden";
        return true;
    }

    if (subcommand != L"create" && subcommand != L"show") {
        statusText = L"Use: ufo create --id saucer --score score:200 --weapon enemy-shot --arrival 7..18 --aim player_ship --edge-spawn";
        return false;
    }
    if (!BgePluginAlreadyImported(L"bge.piece.ufo")) {
        statusText = L"Import UFO first: plugin import bge.piece.ufo";
        return false;
    }

    BgeUfoState ufo;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        ufo = g_ufoState;
    }
    ufo.active = true;

    std::wstring value;
    if (TryGetCommandOptionValue(tokens, L"--id", value)) {
        ufo.id = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--score", value)) {
        std::wstring counterName;
        int points = 0;
        if (BgeUfoScoreSpec(value, counterName, points)) {
            ufo.score = NormalizeTitleScreenText(counterName) + L":" + std::to_wstring(points);
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--weapon", value)) {
        ufo.weapon = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--aim", value)) {
        ufo.aim = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--arrival", value)) {
        float minimum = 0.0f;
        float maximum = 0.0f;
        if (TryParseBgeRange(value, minimum, maximum)) {
            ufo.arrivalMinSeconds = ClampFloat(minimum, 0.5f, 120.0f);
            ufo.arrivalMaxSeconds = ClampFloat(maximum, ufo.arrivalMinSeconds, 180.0f);
        }
        else {
            float seconds = 0.0f;
            if (TryParseFloatArg(value, seconds)) {
                ufo.arrivalMinSeconds = ClampFloat(seconds, 0.5f, 120.0f);
                ufo.arrivalMaxSeconds = ufo.arrivalMinSeconds;
            }
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--radius", value)) {
        float parsedRadius = 0.0f;
        if (TryParseFloatArg(value, parsedRadius)) {
            ufo.radius = ClampFloat(parsedRadius, 8.0f, 80.0f);
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--speed", value)) {
        float parsedSpeed = 0.0f;
        if (TryParseFloatArg(value, parsedSpeed)) {
            ufo.speed = ClampFloat(parsedSpeed, 30.0f, 900.0f);
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--style", value)) {
        BgeObjectRenderStyle parsedStyle = BgeObjectRenderStyle::Outline;
        if (BgeTryParseObjectRenderStyle(LowerArg(value), parsedStyle)) {
            ufo.renderStyle = parsedStyle;
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--outline-thickness", value)) {
        float parsedThickness = 0.0f;
        if (TryParseFloatArg(value, parsedThickness)) {
            ufo.outlineThickness = ClampFloat(parsedThickness, 0.5f, 16.0f);
        }
    }
    ufo.edgeSpawn = HasCommandFlag(tokens, L"--edge-spawn") || !HasCommandFlag(tokens, L"--no-edge-spawn");

    bool spawned = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        g_ufoState = ufo;
        g_ufoActive = true;
        ResetBgeUfoArrivalTimerLocked();
        if (subcommand == L"show") {
            HideBgeUfoLocked();
            spawned = SpawnBgeUfoLocked();
            ResetBgeUfoArrivalTimerLocked();
        }
        g_rendererStateDirty = true;
        PersistActiveObjectGroupLocked();
    }

    InvalidateGameRenderer();
    statusText = BgeUfoStatusText(ufo);
    if (spawned) {
        statusText += L" | spawned";
    }
    LogRendererMessage("[BgeUfoPlugin] configured id=\"" + Narrow(ufo.id) + "\" score=\"" + Narrow(ufo.score) + "\" arrival=" + std::to_string(static_cast<int>(ufo.arrivalMinSeconds)) + ".." + std::to_string(static_cast<int>(ufo.arrivalMaxSeconds)));
    return true;
}

bool TickBgeUfo(double deltaMilliseconds)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    bool dirty = false;
    bool spawned = false;
    bool exited = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (!g_ufoActive || !g_ufoState.active || g_titleScreenActive || g_vectorShipState.gameOver || !g_ballAnimationRunning) {
            return false;
        }

        float deltaSeconds = static_cast<float>((std::max)(0.0, deltaMilliseconds) / 1000.0);
        BgeGameViewport viewport = CurrentGameViewport();
        int activeSlot = ActiveBgeUfoSlotLocked();
        if (activeSlot >= 0) {
            const BgeObjectSlotState& ufo = g_objectSlots[activeSlot];
            float margin = (std::max)(40.0f, ufo.radius * 3.0f);
            if (ufo.x < -margin || ufo.x > viewport.width + margin || ufo.y < viewport.playTop - margin || ufo.y > viewport.playTop + viewport.playHeight + margin) {
                HideBgeUfoSlotLocked(activeSlot);
                ResetBgeUfoArrivalTimerLocked();
                exited = true;
                dirty = true;
            }
        }
        else {
            if (g_ufoState.arrivalRemainingSeconds <= 0.0f) {
                ResetBgeUfoArrivalTimerLocked();
            }
            g_ufoState.arrivalRemainingSeconds = (std::max)(0.0f, g_ufoState.arrivalRemainingSeconds - deltaSeconds);
            if (g_ufoState.arrivalRemainingSeconds <= 0.0f) {
                spawned = SpawnBgeUfoLocked();
                ResetBgeUfoArrivalTimerLocked(spawned ? -1.0f : 3.0f, spawned ? -1.0f : 6.0f);
                dirty = dirty || spawned;
            }
        }

        if (dirty) {
            BgeUpdateCollisionFlags(g_objectSlots);
            PersistActiveObjectGroupLocked();
            g_rendererStateDirty = true;
        }
    }

    if (spawned) {
        SendWorkerTelemetry(L"game-event", L"ufo-spawn", L"saucer entered");
        LogRendererMessage("[BgeUfoPlugin] spawned");
    }
    else if (exited) {
        LogRendererMessage("[BgeUfoPlugin] exited");
    }
    return dirty;
}

// ============================================================
// bge.piece.audio plugin (engine-neutral)
// Attributes contributed by BgeAudioPluginOperation: plugin.bge.piece.audio
// trace-id: lane.asteroids-arcade-fidelity.audio-plugin-skeleton
// ============================================================

struct BgeSoundDefinition {
    std::wstring id;
    std::wstring kind = L"tone";      // tone | noise (future)
    float freq = 440.0f;              // Hz
    float duration = 0.1f;            // seconds
    float volume = 0.6f;              // 0..1
    std::vector<unsigned char> wav;   // synthesized WAV file bytes, ready for SND_MEMORY
};

static std::vector<BgeSoundDefinition> g_soundDefinitions;

void SynthesizeBgeSoundWavLocked(BgeSoundDefinition& def)
{
    // Compose a minimal 16-bit mono PCM RIFF/WAVE file in memory and
    // hand it to PlaySoundW(..., SND_MEMORY | SND_ASYNC). Cheap,
    // dependency-free, audible. Replace with XAudio2 later if mixing
    // becomes a quality blocker.
    const int sampleRate = 22050;
    const int channels = 1;
    const int bitsPerSample = 16;
    const float dur = ClampFloat(def.duration, 0.005f, 4.0f);
    const float vol = ClampFloat(def.volume, 0.0f, 1.0f);
    const int totalSamples = (std::max)(1, static_cast<int>(sampleRate * dur));
    const int byteRate = sampleRate * channels * bitsPerSample / 8;
    const int blockAlign = channels * bitsPerSample / 8;
    const int dataSize = totalSamples * blockAlign;
    const int fmtChunkSize = 16;
    const int riffSize = 4 + (8 + fmtChunkSize) + (8 + dataSize);

    def.wav.clear();
    def.wav.reserve(8 + riffSize);

    auto put32 = [&](unsigned int v) {
        def.wav.push_back(static_cast<unsigned char>(v & 0xFF));
        def.wav.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
        def.wav.push_back(static_cast<unsigned char>((v >> 16) & 0xFF));
        def.wav.push_back(static_cast<unsigned char>((v >> 24) & 0xFF));
    };
    auto put16 = [&](unsigned short v) {
        def.wav.push_back(static_cast<unsigned char>(v & 0xFF));
        def.wav.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
    };
    auto putTag = [&](const char* t) {
        def.wav.push_back(static_cast<unsigned char>(t[0]));
        def.wav.push_back(static_cast<unsigned char>(t[1]));
        def.wav.push_back(static_cast<unsigned char>(t[2]));
        def.wav.push_back(static_cast<unsigned char>(t[3]));
    };

    putTag("RIFF");
    put32(static_cast<unsigned int>(riffSize));
    putTag("WAVE");
    putTag("fmt ");
    put32(static_cast<unsigned int>(fmtChunkSize));
    put16(1);                                       // PCM
    put16(static_cast<unsigned short>(channels));
    put32(static_cast<unsigned int>(sampleRate));
    put32(static_cast<unsigned int>(byteRate));
    put16(static_cast<unsigned short>(blockAlign));
    put16(static_cast<unsigned short>(bitsPerSample));
    putTag("data");
    put32(static_cast<unsigned int>(dataSize));

    const double pi2 = 6.283185307179586;
    const double freq = (std::max)(20.0f, def.freq);
    // Tiny fade-in/out to suppress click artifacts.
    const int fadeSamples = (std::min)(totalSamples / 4, static_cast<int>(sampleRate * 0.005));
    for (int i = 0; i < totalSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double envelope = 1.0;
        if (fadeSamples > 0) {
            if (i < fadeSamples) envelope = static_cast<double>(i) / fadeSamples;
            else if (i > totalSamples - fadeSamples) envelope = static_cast<double>(totalSamples - i) / fadeSamples;
        }
        double sample = sin(pi2 * freq * t) * envelope * vol;
        int s = static_cast<int>(sample * 32760.0);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        put16(static_cast<unsigned short>(static_cast<short>(s)));
    }
}

const BgeSoundDefinition* FindBgeSoundDefinitionLocked(const std::wstring& id)
{
    for (const auto& def : g_soundDefinitions) {
        if (def.id == id) return &def;
    }
    return nullptr;
}

bool ExecuteBgeSoundCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"sound commands run in bge.game-loop";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";

    if (subcommand == L"status" || subcommand == L"list") {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        statusText = L"Sounds defined: " + std::to_wstring(static_cast<int>(g_soundDefinitions.size()));
        return true;
    }

    if (subcommand == L"play") {
        if (!BgePluginAlreadyImported(L"bge.piece.audio")) {
            statusText = L"Import audio first: plugin import bge.piece.audio";
            return false;
        }
        if (tokens.size() < 3) {
            statusText = L"Use: sound play <id>";
            return false;
        }
        std::wstring id = NormalizeTitleScreenText(tokens[2]);
        std::vector<unsigned char> wavCopy;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            const BgeSoundDefinition* def = FindBgeSoundDefinitionLocked(id);
            if (!def || def->wav.empty()) {
                statusText = L"Sound not defined: " + id;
                return false;
            }
            wavCopy = def->wav;
        }
        // PlaySoundW with SND_MEMORY|SND_ASYNC reads from the buffer
        // immediately; safe to let wavCopy fall out of scope after the
        // call. Use SND_NOSTOP=0 so consecutive plays interrupt prior.
        PlaySoundW(reinterpret_cast<LPCWSTR>(wavCopy.data()), nullptr, SND_MEMORY | SND_ASYNC);
        statusText = L"Sound played: " + id;
        return true;
    }

    if (subcommand != L"define" && subcommand != L"create") {
        statusText = L"Use: sound define --id shot --kind tone --freq 880 --duration 0.08 --volume 0.6";
        return false;
    }
    if (!BgePluginAlreadyImported(L"bge.piece.audio")) {
        statusText = L"Import audio first: plugin import bge.piece.audio";
        return false;
    }

    BgeSoundDefinition def;
    std::wstring value;
    if (TryGetCommandOptionValue(tokens, L"--id", value)) {
        def.id = NormalizeTitleScreenText(value);
    }
    if (def.id.empty()) {
        statusText = L"sound define: --id required";
        return false;
    }
    if (TryGetCommandOptionValue(tokens, L"--kind", value)) {
        def.kind = LowerArg(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--freq", value)) {
        float parsed = 0.0f;
        if (TryParseFloatArg(value, parsed)) def.freq = ClampFloat(parsed, 20.0f, 8000.0f);
    }
    if (TryGetCommandOptionValue(tokens, L"--duration", value)) {
        float parsed = 0.0f;
        if (TryParseFloatArg(value, parsed)) def.duration = ClampFloat(parsed, 0.005f, 4.0f);
    }
    if (TryGetCommandOptionValue(tokens, L"--volume", value)) {
        float parsed = 0.0f;
        if (TryParseFloatArg(value, parsed)) def.volume = ClampFloat(parsed, 0.0f, 1.0f);
    }

    SynthesizeBgeSoundWavLocked(def);

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        // Idempotent: replace existing definition with same id.
        bool replaced = false;
        for (auto& existing : g_soundDefinitions) {
            if (existing.id == def.id) { existing = def; replaced = true; break; }
        }
        if (!replaced) g_soundDefinitions.push_back(def);
    }

    statusText = L"Sound defined: " + def.id;
    return true;
}

bool ExecuteBgeProjectileCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"projectile commands run in bge.game-loop";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
    if (subcommand == L"status" || subcommand == L"list") {
        std::vector<BgeProjectileDefinition> definitions;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            definitions = g_projectileDefinitions;
        }
        statusText = ProjectileDefinitionStatusText(definitions);
        return true;
    }

    if (subcommand != L"create" && subcommand != L"define") {
        statusText = L"Use: projectile create --id shot --owner player --shape line --speed 620 --ttl 1.1 --wrap --collision-tag player-shot";
        return false;
    }
    if (!BgePluginAlreadyImported(L"bge.piece.projectile")) {
        statusText = L"Import projectile first: plugin import bge.piece.projectile";
        return false;
    }

    BgeProjectileDefinition definition;
    std::wstring value;
    if (TryGetCommandOptionValue(tokens, L"--id", value)) {
        definition.id = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--owner", value)) {
        definition.owner = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--shape", value)) {
        BgeObjectShape parsedShape = BgeObjectShape::Line;
        if (BgeTryParseObjectShape(LowerArg(value), parsedShape)) {
            definition.shape = parsedShape;
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--speed", value)) {
        float parsedSpeed = 0.0f;
        if (TryParseFloatArg(value, parsedSpeed)) {
            definition.speed = ClampFloat(parsedSpeed, 1.0f, 2400.0f);
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--ttl", value)) {
        float parsedTtl = 0.0f;
        if (TryParseFloatArg(value, parsedTtl)) {
            definition.ttlSeconds = ClampFloat(parsedTtl, 0.05f, 30.0f);
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--collision-tag", value)) {
        definition.collisionTag = NormalizeTitleScreenText(value);
    }
    definition.wrap = HasCommandFlag(tokens, L"--wrap") || !HasCommandFlag(tokens, L"--no-wrap");

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        BgeProjectileDefinition* existing = FindBgeProjectileDefinitionMutable(definition.id);
        if (existing) {
            *existing = definition;
        }
        else {
            g_projectileDefinitions.push_back(definition);
        }
        if (definition.wrap) {
            BgeSetCurrentEdgePolicy(BgeEdgePolicy::Wrap);
        }
        g_rendererStateDirty = true;
    }

    statusText = L"Projectile defined: " + definition.id
        + L" owner=" + definition.owner
        + L" shape=" + BgeObjectShapeName(definition.shape)
        + L" speed=" + std::to_wstring(static_cast<int>(definition.speed))
        + L" ttl=" + std::to_wstring(definition.ttlSeconds);
    LogRendererMessage("[BgeProjectilePlugin] grammar-extended id=\"" + Narrow(definition.id) + "\" owner=\"" + Narrow(definition.owner) + "\" shape=\"" + Narrow(BgeObjectShapeName(definition.shape)) + "\"");
    return true;
}

bool TickBgeProjectiles(double deltaMilliseconds)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    bool dirty = false;
    int hitCount = 0;
    int ufoHitCount = 0;
    int scoreDelta = 0;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        float deltaSeconds = static_cast<float>((std::max)(0.0, deltaMilliseconds) / 1000.0);
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            BgeObjectSlotState& slot = g_objectSlots[index];
            if (slot.kind != BgeObjectKind::Bullet || g_bgeProjectileLifeSeconds[index] <= 0.0f) {
                continue;
            }
            g_bgeProjectileLifeSeconds[index] = (std::max)(0.0f, g_bgeProjectileLifeSeconds[index] - deltaSeconds);
            if (g_bgeProjectileLifeSeconds[index] <= 0.0f) {
                slot.visible = false;
                slot.deleteMarked = false;
                slot.isDeleted = false;
                slot.collisionDetected = false;
                slot.kind = BgeObjectKind::Generic;
                dirty = true;
            }
        }

        for (int projectileIndex = 0; projectileIndex < BGE_OBJECT_SLOT_COUNT; ++projectileIndex) {
            BgeObjectSlotState& projectile = g_objectSlots[projectileIndex];
            if (!projectile.visible || projectile.kind != BgeObjectKind::Bullet || g_bgeProjectileLifeSeconds[projectileIndex] <= 0.0f) {
                continue;
            }

            for (int ufoIndex = 0; ufoIndex < BGE_OBJECT_SLOT_COUNT; ++ufoIndex) {
                BgeObjectSlotState& ufo = g_objectSlots[ufoIndex];
                if (!ufo.visible || ufo.kind != BgeObjectKind::Ufo || !BgeObjectSlotsOverlap(projectile, ufo)) {
                    continue;
                }

                projectile.visible = false;
                projectile.deleteMarked = false;
                projectile.isDeleted = false;
                projectile.collisionDetected = false;
                projectile.kind = BgeObjectKind::Generic;
                g_bgeProjectileLifeSeconds[projectileIndex] = 0.0f;

                HideBgeUfoSlotLocked(ufoIndex);
                AwardBgeUfoScoreLocked(scoreDelta);
                ResetBgeUfoArrivalTimerLocked();
                ++ufoHitCount;
                dirty = true;
                break;
            }
        }

        if (g_breakableRockFieldActive) {
            for (int projectileIndex = 0; projectileIndex < BGE_OBJECT_SLOT_COUNT; ++projectileIndex) {
                BgeObjectSlotState& projectile = g_objectSlots[projectileIndex];
                if (!projectile.visible || projectile.kind != BgeObjectKind::Bullet || g_bgeProjectileLifeSeconds[projectileIndex] <= 0.0f) {
                    continue;
                }

                for (int rockIndex = 0; rockIndex < BGE_OBJECT_SLOT_COUNT; ++rockIndex) {
                    BgeObjectSlotState& rock = g_objectSlots[rockIndex];
                    if (!rock.visible || rock.kind != BgeObjectKind::Asteroid || !BgeObjectSlotsOverlap(projectile, rock)) {
                        continue;
                    }

                    BgeObjectSlotState sourceRock = rock;
                    int sizeIndex = BgeRockSizeIndexForRadius(g_breakableRockFieldState, sourceRock.radius);
                    std::wstring scoreCounter;
                    int points = BgeRockScoreForSizeIndex(g_breakableRockFieldState, sizeIndex, scoreCounter);
                    if (points != 0 && AddBgeCounterValueLocked(scoreCounter, points)) {
                        scoreDelta += points;
                    }

                    projectile.visible = false;
                    projectile.deleteMarked = false;
                    projectile.isDeleted = false;
                    projectile.collisionDetected = false;
                    projectile.kind = BgeObjectKind::Generic;
                    g_bgeProjectileLifeSeconds[projectileIndex] = 0.0f;

                    rock.visible = false;
                    rock.deleteMarked = false;
                    rock.isDeleted = false;
                    rock.collisionDetected = false;
                    rock.kind = BgeObjectKind::Generic;

                    int nextSizeIndex = sizeIndex + 1;
                    if (g_breakableRockFieldState.splitCount > 0 && nextSizeIndex < static_cast<int>(g_breakableRockFieldState.sizes.size())) {
                        int splitTotal = (std::max)(1, g_breakableRockFieldState.splitCount);
                        for (int splitIndex = 0; splitIndex < splitTotal; ++splitIndex) {
                            int splitSlot = splitIndex == 0 ? rockIndex : FindReusableBgeRockSlotLocked();
                            if (splitSlot < 0) {
                                break;
                            }
                            ConfigureBgeSplitRockSlotLocked(splitSlot, g_breakableRockFieldState, sourceRock, g_breakableRockFieldState.sizes[nextSizeIndex], splitIndex, splitTotal);
                        }
                    }

                    ++hitCount;
                    dirty = true;
                    break;
                }
            }
        }

        if (dirty && g_breakableRockFieldActive) {
            int remainingRocks = 0;
            for (const BgeObjectSlotState& slot : g_objectSlots) {
                if (slot.visible && slot.kind == BgeObjectKind::Asteroid) {
                    ++remainingRocks;
                }
            }
            g_breakableRockFieldState.count = remainingRocks;
            if (remainingRocks == 0) {
                g_breakableRockFieldActive = false;
                g_breakableRockFieldState.active = false;
                int nextWave = 2;
                if (!g_breakableRockFieldState.waveCounter.empty()) {
                    AddBgeCounterValueLocked(g_breakableRockFieldState.waveCounter, 1);
                    if (const BgeCounterState* waveCounter = FindBgeCounterMutable(g_breakableRockFieldState.waveCounter)) {
                        nextWave = (std::max)(1, waveCounter->value);
                    }
                }
                // Keep the game alive: respawn the next wave so the player
                // doesn't end up on an empty board with nothing to do.
                SpawnSpaceRocksWaveLocked(nextWave);
                SendWorkerTelemetry(L"game-event", L"wave-cleared", L"next wave spawned");
            }
        }

        if (dirty) {
            BgeUpdateCollisionFlags(g_objectSlots);
            PersistActiveObjectGroupLocked();
            g_rendererStateDirty = true;
        }
    }
    if (hitCount > 0) {
        std::wstring statusText = L"Rock hit: " + std::to_wstring(hitCount) + L" score +" + std::to_wstring(scoreDelta);
        SendWorkerTelemetry(L"game-event", L"rock-field collision", statusText);
        LogRendererMessage("[BgeBreakableRockFieldPlugin] projectile-hit hits=" + std::to_string(hitCount) + " scoreDelta=" + std::to_string(scoreDelta));
    }
    if (ufoHitCount > 0) {
        std::wstring statusText = L"UFO hit: " + std::to_wstring(ufoHitCount) + L" score +" + std::to_wstring(scoreDelta);
        SendWorkerTelemetry(L"game-event", L"ufo collision", statusText);
        LogRendererMessage("[BgeUfoPlugin] projectile-hit hits=" + std::to_string(ufoHitCount) + " scoreDelta=" + std::to_string(scoreDelta));
    }
    return dirty;
}

float VectorShipLength(float x, float y)
{
    return std::sqrt(x * x + y * y);
}

void NormalizeVectorShipDirection(float x, float y, float& outX, float& outY)
{
    float length = VectorShipLength(x, y);
    if (length < 1.0f) {
        outX = 1.0f;
        outY = 0.0f;
        return;
    }
    outX = x / length;
    outY = y / length;
}

void ClampVectorShipVelocity(BgeObjectSlotState& slot, float maxSpeed)
{
    float speed = VectorShipLength(slot.velocityX, slot.velocityY);
    if (speed > maxSpeed && speed > 0.0f) {
        float scale = maxSpeed / speed;
        slot.velocityX *= scale;
        slot.velocityY *= scale;
    }
}

bool VectorShipPlayerAliveLocked()
{
    return g_vectorShipActive
        && g_vectorShipState.active
        && g_vectorShipState.slotIndex >= 0
        && g_vectorShipState.slotIndex < BGE_OBJECT_SLOT_COUNT
        && g_mainPlayerGroupIndex == g_activeObjectGroupIndex
        && g_mainPlayerSlot == g_vectorShipState.slotIndex
        && g_objectSlots[g_vectorShipState.slotIndex].visible
        && !g_objectSlots[g_vectorShipState.slotIndex].isDeleted
        && g_objectSlots[g_vectorShipState.slotIndex].kind == BgeObjectKind::Player;
}

void ApplyVectorShipHeadingModeLocked(BgeObjectSlotState& slot, const BgeVectorShipState& ship)
{
    float headingRadians = ship.headingDegrees * 3.14159265358979323846f / 180.0f;
    slot.headingX = std::cos(headingRadians);
    slot.headingY = std::sin(headingRadians);
    BgeApplyPlayerIconHeadingMode(slot, slot.headingX, slot.headingY, ship.playerIconVisibilityMode);
}

std::wstring VectorShipStatusText(const BgeVectorShipState& ship)
{
    if (!ship.active) {
        return L"Player ship hidden";
    }
    return L"Player ship: " + ship.id
        + L" | object " + std::to_wstring(ship.slotIndex + 1)
        + L" | shape " + BgeObjectShapeName(ship.shape)
        + L" | icon " + std::to_wstring(BgeNormalizePlayerIconVisibilityModeIndex(ship.playerIconVisibilityMode))
        + L" " + BgePlayerIconVisibilityModeName(ship.playerIconVisibilityMode)
        + L" | lives " + ship.lives
        + L" | fire " + ship.fire
        + L" | input " + ship.inputProfile;
}

void ConfigureVectorShipSlotLocked(const BgeVectorShipState& ship, const BgeGameViewport& viewport)
{
    BgeObjectSlotState& slot = g_objectSlots[ship.slotIndex];
    slot = BgeObjectSlotState{};
    slot.visible = true;
    slot.x = viewport.width * 0.50f;
    slot.y = viewport.playTop + viewport.playHeight * 0.50f;
    slot.radius = 14.0f;
    // Asteroids feel: ship spawns stationary, pointing straight up.
    slot.velocityX = 0.0f;
    slot.velocityY = 0.0f;
    ApplyVectorShipHeadingModeLocked(slot, ship);
    // Monochrome white vector ship matches the original arcade look.
    slot.colorR = 1.0f;
    slot.colorG = 1.0f;
    slot.colorB = 1.0f;
    slot.colorA = ship.invulnerableRemainingSeconds > 0.0f ? 0.54f : 1.0f;
    slot.shape = ship.shape;
    slot.kind = BgeObjectKind::Player;
    slot.renderStyle = ship.renderStyle;
    slot.outlineThickness = ship.outlineThickness;
    BgeApplyPlayerIconVisibilityMode(slot, ship.playerIconVisibilityMode, ship.invulnerableRemainingSeconds > 0.0f);
}

bool ExecuteBgeVectorShipCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"player-ship commands run in bge.game-loop";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
    if (subcommand == L"status") {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        statusText = VectorShipStatusText(g_vectorShipState);
        return true;
    }

    if (subcommand == L"hide" || subcommand == L"clear") {
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            if (g_vectorShipState.slotIndex >= 0 && g_vectorShipState.slotIndex < BGE_OBJECT_SLOT_COUNT) {
                BgeObjectSlotState& slot = g_objectSlots[g_vectorShipState.slotIndex];
                slot.visible = false;
                slot.kind = BgeObjectKind::Generic;
            }
            g_vectorShipState.active = false;
            g_vectorShipActive = false;
            g_vectorShipInputState = BgeVectorShipInputState{};
            g_rendererStateDirty = true;
            PersistActiveObjectGroupLocked();
        }
        SyncBallControls();
        InvalidateGameRenderer();
        statusText = L"Player ship hidden";
        return true;
    }

    if (subcommand == L"select" || subcommand == L"focus") {
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            if (!VectorShipPlayerAliveLocked()) {
                statusText = L"Player ship unavailable";
                return false;
            }
            g_selectedObjectSlot = g_vectorShipState.slotIndex;
            g_objectSelectionActive = true;
            SetGameObjectKeyboardFocusLocked();
            RefreshSelectedObjectGlobalsLocked();
            PersistActiveObjectGroupLocked();
        }
        SyncBallControls();
        statusText = L"Player ship selected";
        return true;
    }

    if (subcommand != L"create" && subcommand != L"show") {
        statusText = L"Use: player-ship create --id player_ship --shape vector-ship --lives lives --input-profile arrows-space --fire shot --hyperspace H --respawn 1.5 --invulnerable 2.0";
        return false;
    }
    if (!BgePluginAlreadyImported(L"bge.piece.vector-ship")) {
        statusText = L"Import vector-ship first: plugin import bge.piece.vector-ship";
        return false;
    }

    BgeVectorShipState ship;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        ship = g_vectorShipState;
    }
    ship.active = true;
    ship.slotIndex = 0;
    ship.shape = BgeObjectShape::VectorShip;
    ship.playerIconVisibilityMode = BgeNormalizePlayerIconVisibilityModeIndex(ship.playerIconVisibilityMode);

    std::wstring value;
    if (TryGetCommandOptionValue(tokens, L"--id", value)) {
        ship.id = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--shape", value)) {
        BgeObjectShape parsedShape = BgeObjectShape::VectorShip;
        if (BgeTryParseObjectShape(LowerArg(value), parsedShape)) {
            ship.shape = parsedShape;
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--lives", value)) {
        ship.lives = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--input-profile", value)) {
        ship.inputProfile = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--fire", value)) {
        ship.fire = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--hyperspace", value)) {
        ship.hyperspace = NormalizeTitleScreenText(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--respawn", value)) {
        float parsedRespawn = 0.0f;
        if (TryParseFloatArg(value, parsedRespawn)) {
            ship.respawnSeconds = ClampFloat(parsedRespawn, 0.0f, 12.0f);
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--invulnerable", value)) {
        float parsedInvulnerable = 0.0f;
        if (TryParseFloatArg(value, parsedInvulnerable)) {
            ship.invulnerableSeconds = ClampFloat(parsedInvulnerable, 0.0f, 12.0f);
        }
    }
    // Recipe-driven render style. Default Filled keeps legacy look;
    // classic-Asteroids witness recipes opt in with --style outline.
    // Engine-neutral: any plugin authoring a slot can read this.
    if (TryGetCommandOptionValue(tokens, L"--style", value)) {
        BgeObjectRenderStyle parsedStyle = BgeObjectRenderStyle::Filled;
        if (BgeTryParseObjectRenderStyle(LowerArg(value), parsedStyle)) {
            ship.renderStyle = parsedStyle;
        }
    }
    if (TryGetCommandOptionValue(tokens, L"--outline-thickness", value)) {
        float parsedThickness = 0.0f;
        if (TryParseFloatArg(value, parsedThickness)) {
            ship.outlineThickness = ClampFloat(parsedThickness, 0.5f, 16.0f);
        }
    }
    ship.invulnerableRemainingSeconds = ship.invulnerableSeconds;

    BgeGameViewport viewport = CurrentGameViewport();
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        g_vectorShipState = ship;
        g_vectorShipActive = true;
        g_vectorShipInputState = BgeVectorShipInputState{};
        g_mainPlayerGroupIndex = g_activeObjectGroupIndex;
        g_mainPlayerSlot = ship.slotIndex;
        g_selectedObjectSlot = ship.slotIndex;
        g_objectSelectionActive = true;
        ConfigureVectorShipSlotLocked(g_vectorShipState, viewport);
        // Make sure the player starts with 3 lives even if the author
        // forgot to declare the counter. Idempotent: existing value wins.
        std::wstring livesName = g_vectorShipState.lives.empty() ? L"lives" : g_vectorShipState.lives;
        EnsureBgeCounterLocked(livesName);
        if (const BgeCounterState* livesCounter = FindBgeCounterMutable(livesName)) {
            if (livesCounter->value <= 0) {
                AddBgeCounterValueLocked(livesName, 3 - livesCounter->value);
            }
        }
        SetGameObjectKeyboardFocusLocked();
        RefreshSelectedObjectGlobalsLocked();
        BgeUpdateCollisionFlags(g_objectSlots);
        PersistActiveObjectGroupLocked();
        g_ballAnimationRunning = true;
        g_rendererStateDirty = true;
    }

    SyncBallControls();
    InvalidateGameRenderer();
    statusText = VectorShipStatusText(ship);
    LogRendererMessage("[BgeVectorShipPlugin] entity-spawned id=\"" + Narrow(ship.id) + "\" slot=" + std::to_string(ship.slotIndex + 1) + " input=\"" + Narrow(ship.inputProfile) + "\"");
    return true;
}

bool HandleBgeVectorShipKeyDown(WPARAM key)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    int visibilityMode = BgePlayerIconVisibilityModeIndexFromKey(static_cast<unsigned int>(key));
    bool shipKey = visibilityMode >= 0 || key == L'A' || key == L'D' || key == L'W' || key == L'S' || key == L'H' || key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_SPACE;
    if (!shipKey) {
        return false;
    }

    if (visibilityMode >= 0) {
        std::wstring statusText;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            if (!VectorShipPlayerAliveLocked()) {
                return false;
            }
            g_vectorShipState.playerIconVisibilityMode = BgeNormalizePlayerIconVisibilityModeIndex(visibilityMode);
            BgeObjectSlotState& shipSlot = g_objectSlots[g_vectorShipState.slotIndex];
            BgeApplyPlayerIconVisibilityMode(shipSlot, g_vectorShipState.playerIconVisibilityMode, g_vectorShipState.invulnerableRemainingSeconds > 0.0f);
            ApplyVectorShipHeadingModeLocked(shipSlot, g_vectorShipState);
            g_selectedObjectSlot = g_vectorShipState.slotIndex;
            g_objectSelectionActive = true;
            g_ballColorR = shipSlot.colorR;
            g_ballColorG = shipSlot.colorG;
            g_ballColorB = shipSlot.colorB;
            g_ballColorA = shipSlot.colorA;
            g_rendererStateDirty = true;
            RefreshSelectedObjectGlobalsLocked();
            PersistActiveObjectGroupLocked();
            statusText = L"Player ship: icon mode " + std::to_wstring(g_vectorShipState.playerIconVisibilityMode)
                + L" (" + BgePlayerIconVisibilityModeName(g_vectorShipState.playerIconVisibilityMode) + L")";
        }
        SyncBallControls();
        InvalidateGameRenderer();
        SetCommandStatus(statusText);
        SendWorkerTelemetry(L"player-icon-mode", L"player-ship visibility", statusText);
        LogRendererMessage("[BgeVectorShipPlugin] player-icon-mode status=\"" + Narrow(statusText) + "\"");
        return true;
    }

    bool heldInputKey = key == L'A' || key == L'D' || key == L'W' || key == L'S' || key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN;
    if (heldInputKey) {
        bool changed = false;
        std::wstring statusText;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            if (!VectorShipPlayerAliveLocked()) {
                return false;
            }
            auto setPressed = [&changed](bool& flag) {
                if (!flag) {
                    flag = true;
                    changed = true;
                }
            };
            if (key == L'A' || key == VK_LEFT) {
                setPressed(g_vectorShipInputState.turnLeft);
                statusText = L"Player ship: turn left held";
            }
            else if (key == L'D' || key == VK_RIGHT) {
                setPressed(g_vectorShipInputState.turnRight);
                statusText = L"Player ship: turn right held";
            }
            else if (key == L'W' || key == VK_UP) {
                setPressed(g_vectorShipInputState.thrust);
                statusText = L"Player ship: thrust held";
            }
            else {
                setPressed(g_vectorShipInputState.reverse);
                statusText = L"Player ship: reverse held";
            }
            g_ballAnimationRunning = true;
        }
        if (changed) {
            SetCommandStatus(statusText);
            SendWorkerTelemetry(L"player-input-down", L"player-ship input", statusText);
            // DIAGNOSTIC: prove key transitions reach the worker's ship-input state.
            LogRendererMessage("[ShipInput] DOWN key=" + std::to_string(static_cast<int>(key))
                + " turnLeft=" + std::to_string(g_vectorShipInputState.turnLeft ? 1 : 0)
                + " turnRight=" + std::to_string(g_vectorShipInputState.turnRight ? 1 : 0)
                + " thrust=" + std::to_string(g_vectorShipInputState.thrust ? 1 : 0));
        }
        return true;
    }

    bool handled = false;
    bool dirty = false;
    std::wstring statusText;
    BgeGameViewport viewport = CurrentGameViewport();
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (!VectorShipPlayerAliveLocked()) {
            return false;
        }

        BgeObjectSlotState& shipSlot = g_objectSlots[g_vectorShipState.slotIndex];

        if (key == L'H') {
            float minX = shipSlot.radius;
            float maxX = (std::max)(minX, viewport.width - shipSlot.radius);
            float minY = viewport.playTop + shipSlot.radius;
            float maxY = (std::max)(minY, viewport.playTop + viewport.playHeight - shipSlot.radius);
            float unitX = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            float unitY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            shipSlot.x = minX + (maxX - minX) * unitX;
            shipSlot.y = minY + (maxY - minY) * unitY;
            g_vectorShipState.invulnerableRemainingSeconds = g_vectorShipState.invulnerableSeconds;
            shipSlot.colorA = g_vectorShipState.invulnerableRemainingSeconds > 0.0f ? 0.54f : 1.0f;
            statusText = L"Player ship: hyperspace";
            handled = true;
            dirty = true;
        }
        else if (key == VK_SPACE) {
            dirty = FireBgeProjectileFromVectorShipLocked(statusText);
            handled = true;
        }

        if (dirty) {
            g_ballVelocityX = shipSlot.velocityX;
            g_ballVelocityY = shipSlot.velocityY;
            g_ballAnimationRunning = true;
            g_rendererStateDirty = true;
            BgeUpdateCollisionFlags(g_objectSlots);
            RefreshSelectedObjectGlobalsLocked();
            PersistActiveObjectGroupLocked();
        }
    }

    if (!handled) {
        return false;
    }
    if (dirty) {
        SyncBallControls();
        InvalidateGameRenderer();
    }
    SetCommandStatus(statusText);
    SendWorkerTelemetry(L"player-input", L"player-ship input", statusText);
    LogRendererMessage("[BgeVectorShipPlugin] input-bound status=\"" + Narrow(statusText) + "\"");
    return true;
}

bool HandleBgeVectorShipKeyUp(WPARAM key)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    bool heldInputKey = key == L'A' || key == L'D' || key == L'W' || key == L'S' || key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN;
    if (!heldInputKey) {
        return false;
    }

    bool changed = false;
    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        auto setReleased = [&changed](bool& flag) {
            if (flag) {
                flag = false;
                changed = true;
            }
        };
        if (key == L'A' || key == VK_LEFT) {
            setReleased(g_vectorShipInputState.turnLeft);
            statusText = L"Player ship: turn left released";
        }
        else if (key == L'D' || key == VK_RIGHT) {
            setReleased(g_vectorShipInputState.turnRight);
            statusText = L"Player ship: turn right released";
        }
        else if (key == L'W' || key == VK_UP) {
            setReleased(g_vectorShipInputState.thrust);
            statusText = L"Player ship: thrust released";
        }
        else {
            setReleased(g_vectorShipInputState.reverse);
            statusText = L"Player ship: reverse released";
        }
    }
    if (changed) {
        SetCommandStatus(statusText);
        SendWorkerTelemetry(L"player-input-up", L"player-ship input", statusText);
    }
    return true;
}

void ClearBgeVectorShipInputState()
{
    std::lock_guard<std::mutex> lock(ballConfigMutex);
    g_vectorShipInputState = BgeVectorShipInputState{};
}

// ---------------------------------------------------------------------------
// Space Rocks lifecycle helpers (ship death / respawn / restart).
// All "*Locked" helpers below assume the caller already holds ballConfigMutex.
// ---------------------------------------------------------------------------

void SetBgeCounterValueLocked(const std::wstring& name, int value)
{
    if (name.empty()) {
        return;
    }
    EnsureBgeCounterLocked(name);
    if (BgeCounterState* counter = FindBgeCounterMutable(name)) {
        counter->value = ClampCounterValue(*counter, value);
    }
}

void RespawnVectorShipLocked()
{
    BgeGameViewport viewport = CurrentGameViewport();
    g_vectorShipState.awaitingRespawn = false;
    g_vectorShipState.respawnRemainingSeconds = 0.0f;
    g_vectorShipState.headingDegrees = -90.0f;
    g_vectorShipState.invulnerableRemainingSeconds = g_vectorShipState.invulnerableSeconds;
    g_vectorShipActive = true;
    g_vectorShipState.active = true;
    ConfigureVectorShipSlotLocked(g_vectorShipState, viewport);
    BgeUpdateCollisionFlags(g_objectSlots);
    g_ballAnimationRunning = true;
    g_rendererStateDirty = true;
}

void ShowSpaceRocksTitleScreenLocked()
{
    g_titleScreenState.visible = true;
    g_titleScreenState.text = L"ASTEROIDS";
    g_titleScreenState.subtitle = L"PRESS ENTER";
    g_titleScreenActive = true;
    g_rendererStateDirty = true;
}

void ShowSpaceRocksGameOverScreenLocked()
{
    g_titleScreenState.visible = true;
    g_titleScreenState.text = L"GAME OVER";
    g_titleScreenState.subtitle = L"PRESS ENTER";
    g_titleScreenActive = true;
    g_vectorShipState.gameOver = true;
    HideBgeUfoLocked();
    g_rendererStateDirty = true;
}

bool SpawnSpaceRocksWaveLocked(int waveIndex)
{
    // DIAGNOSTIC: trace every wave-spawn so the renderer log shows how
    // many waves were forced. Pairs with StartOrRestartSpaceRocksGameLocked.
    static std::atomic<int> s_waveSpawnCount{0};
    int n = ++s_waveSpawnCount;
    LogRendererMessage("[SpaceRocks] SpawnSpaceRocksWaveLocked wave=" + std::to_string(waveIndex) + " call#" + std::to_string(n));
    SendWorkerTelemetry(L"game-event", L"wave-spawn", L"wave=" + std::to_wstring(waveIndex) + L" call=" + std::to_wstring(n));

    BgeBreakableRockFieldState field = g_breakableRockFieldState;
    // First wave: ensure a baseline count if author never spawned rocks.
    if (field.count <= 0) {
        field.count = 4;
    }
    // Light progression: each subsequent wave adds two rocks (Asteroids-ish).
    int rockCount = (std::max)(2, field.count + (std::max)(0, waveIndex - 1) * 2);
    BgeGameViewport viewport = CurrentGameViewport();

    for (BgeObjectSlotState& slot : g_objectSlots) {
        if (slot.kind == BgeObjectKind::Asteroid) {
            slot.visible = false;
            slot.kind = BgeObjectKind::Generic;
        }
    }

    int spawned = 0;
    for (int rockIndex = 0; rockIndex < rockCount; ++rockIndex) {
        int slotIndex = FindReusableBgeRockSlotLocked();
        if (slotIndex < 0) break;
        ConfigureBgeRockSlotLocked(slotIndex, field, rockIndex, viewport);
        ++spawned;
    }
    field.count = spawned;
    g_breakableRockFieldState = field;
    g_breakableRockFieldActive = spawned > 0;
    BgeUpdateCollisionFlags(g_objectSlots);
    g_rendererStateDirty = true;
    return spawned > 0;
}

void StartOrRestartSpaceRocksGameLocked()
{
    // DIAGNOSTIC: count how many times this gets called per session so
    // we can confirm whether the "game resets every few seconds" bug is
    // a re-entrant restart or something else (collision, wave-spawn, ...).
    static std::atomic<int> s_restartCount{0};
    int n = ++s_restartCount;
    LogRendererMessage("[SpaceRocks] StartOrRestartSpaceRocksGameLocked count=" + std::to_string(n));
    SendWorkerTelemetry(L"game-event", L"start-or-restart", L"count=" + std::to_wstring(n));

    // Reset counters used by the scoreboard overlay.
    SetBgeCounterValueLocked(L"score", 0);
    SetBgeCounterValueLocked(g_vectorShipState.lives.empty() ? L"lives" : g_vectorShipState.lives, 3);
    SetBgeCounterValueLocked(L"wave", 1);

    // Clear bullets so the field starts empty.
    for (BgeObjectSlotState& slot : g_objectSlots) {
        if (slot.kind == BgeObjectKind::Bullet) {
            slot.visible = false;
            slot.kind = BgeObjectKind::Generic;
        }
    }
    HideBgeUfoLocked();
    if (g_ufoActive && g_ufoState.active) {
        ResetBgeUfoArrivalTimerLocked(3.0f, 8.0f);
    }

    g_vectorShipState.gameOver = false;
    g_vectorShipState.awaitingRespawn = false;
    g_vectorShipState.respawnRemainingSeconds = 0.0f;

    // Hide title / game-over screen.
    g_titleScreenState.visible = false;
    g_titleScreenActive = false;

    // Reset ship at centre, pointing up, briefly invulnerable.
    RespawnVectorShipLocked();

    // Spawn wave 1.
    SpawnSpaceRocksWaveLocked(1);

    PersistActiveObjectGroupLocked();
}

void ResolveBgeShipRockCollisionsLocked()
{
    if (!VectorShipPlayerAliveLocked()) return;
    if (g_vectorShipState.invulnerableRemainingSeconds > 0.0f) return;
    if (g_vectorShipState.awaitingRespawn) return;

    BgeObjectSlotState& shipSlot = g_objectSlots[g_vectorShipState.slotIndex];
    for (int i = 0; i < BGE_OBJECT_SLOT_COUNT; ++i) {
        BgeObjectSlotState& other = g_objectSlots[i];
        if (i == g_vectorShipState.slotIndex) continue;
        if (!other.visible) continue;
        if (other.kind != BgeObjectKind::Asteroid && other.kind != BgeObjectKind::Ufo) continue;
        if (!BgeObjectSlotsOverlap(shipSlot, other)) continue;

        // Hit! Hide ship, decrement lives, schedule respawn or game-over.
        shipSlot.visible = false;
        std::wstring livesName = g_vectorShipState.lives.empty() ? L"lives" : g_vectorShipState.lives;
        AddBgeCounterValueLocked(livesName, -1);
        const BgeCounterState* livesCounter = FindBgeCounterMutable(livesName);
        int remainingLives = livesCounter ? livesCounter->value : 0;

        g_vectorShipState.awaitingRespawn = true;
        g_vectorShipState.respawnRemainingSeconds = g_vectorShipState.respawnSeconds;
        g_vectorShipState.invulnerableRemainingSeconds = 0.0f;

        if (remainingLives <= 0) {
            g_vectorShipState.gameOver = true;
            SendWorkerTelemetry(L"game-event", L"game-over", L"lives=0");
        } else {
            SendWorkerTelemetry(L"game-event", L"life-lost", other.kind == BgeObjectKind::Ufo ? L"ship destroyed by ufo" : L"ship destroyed by asteroid");
        }
        BgeUpdateCollisionFlags(g_objectSlots);
        return;
    }
}

bool TickBgeVectorShip(double deltaMilliseconds)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(ballConfigMutex);

    if (!g_vectorShipActive || !g_vectorShipState.active) {
        // Do NOT wipe input here. Keys held down should remain pressed across
        // brief inactive windows (recipe load, plugin reload, etc.); wiping
        // every frame caused turn-only input to be eaten because the OS
        // auto-repeat couldn't keep up with the 60Hz tick. The tick simply
        // skips its work in this state; held flags resume effect on return.
        return false;
    }

    float deltaSeconds = static_cast<float>((std::max)(0.0, deltaMilliseconds) / 1000.0);
    bool dirty = false;

    // Ship is dead and waiting to respawn (lives still remaining).
    if (g_vectorShipState.awaitingRespawn) {
        // Keep held-key input intact across the respawn window. If the player
        // is holding LEFT/RIGHT when the new ship spawns, classic Asteroids
        // resumes rotating immediately - wiping every frame here was the
        // bug that ate turn-only input between OS auto-repeats.
        if (g_vectorShipState.respawnRemainingSeconds > 0.0f) {
            g_vectorShipState.respawnRemainingSeconds = (std::max)(0.0f, g_vectorShipState.respawnRemainingSeconds - deltaSeconds);
            dirty = true;
        }
        if (g_vectorShipState.respawnRemainingSeconds <= 0.0f && !g_vectorShipState.gameOver) {
            RespawnVectorShipLocked();
            dirty = true;
        }
        if (dirty) {
            g_rendererStateDirty = true;
            PersistActiveObjectGroupLocked();
        }
        return dirty;
    }

    if (!VectorShipPlayerAliveLocked()) {
        if (g_vectorShipInputState.turnLeft || g_vectorShipInputState.turnRight || g_vectorShipInputState.thrust) {
            const auto& s = g_objectSlots[g_vectorShipState.slotIndex >= 0 && g_vectorShipState.slotIndex < BGE_OBJECT_SLOT_COUNT ? g_vectorShipState.slotIndex : 0];
            LogRendererMessage(std::string("[ShipSkip] !PlayerAlive (input kept)")
                + " vsAct=" + std::to_string(g_vectorShipActive ? 1 : 0)
                + " stAct=" + std::to_string(g_vectorShipState.active ? 1 : 0)
                + " slotIdx=" + std::to_string(g_vectorShipState.slotIndex)
                + " mainGrp=" + std::to_string(g_mainPlayerGroupIndex)
                + " actGrp=" + std::to_string(g_activeObjectGroupIndex)
                + " mainSlot=" + std::to_string(g_mainPlayerSlot)
                + " vis=" + std::to_string(s.visible ? 1 : 0)
                + " del=" + std::to_string(s.isDeleted ? 1 : 0)
                + " kind=" + std::to_string(static_cast<int>(s.kind)));
        }
        // Ship slot got cleared by something else (hide/select/etc).
        // Do NOT wipe input - see comments above.
        return false;
    }

    // Ship-vs-rock contact may flip the ship to awaitingRespawn or gameOver.
    ResolveBgeShipRockCollisionsLocked();
    if (g_vectorShipState.awaitingRespawn || !VectorShipPlayerAliveLocked()) {
        if (g_vectorShipState.gameOver) {
            ShowSpaceRocksGameOverScreenLocked();
        }
        g_rendererStateDirty = true;
        PersistActiveObjectGroupLocked();
        return true;
    }

    BgeObjectSlotState& slot = g_objectSlots[g_vectorShipState.slotIndex];

    // --- Rotation: turn heading independent of velocity (real Asteroids feel) ---
    int turnDirection = (g_vectorShipInputState.turnRight ? 1 : 0) - (g_vectorShipInputState.turnLeft ? 1 : 0);
    // DIAGNOSTIC: log input + heading once per ~60 frames so we can see whether
    // turn input actually reaches the tick and whether heading is updating.
    {
        static int s_tickLogCounter = 0;
        if ((++s_tickLogCounter % 60) == 0) {
            LogRendererMessage("[ShipTick] turnL=" + std::to_string(g_vectorShipInputState.turnLeft ? 1 : 0)
                + " turnR=" + std::to_string(g_vectorShipInputState.turnRight ? 1 : 0)
                + " thrust=" + std::to_string(g_vectorShipInputState.thrust ? 1 : 0)
                + " heading=" + std::to_string(g_vectorShipState.headingDegrees)
                + " dt=" + std::to_string(deltaSeconds));
        }
    }
    if (turnDirection != 0 && deltaSeconds > 0.0f) {
        float deltaDegrees = static_cast<float>(turnDirection) * g_vectorShipState.turnDegrees * deltaSeconds;
        g_vectorShipState.headingDegrees += deltaDegrees;
        // Normalize to [-180, 180].
        while (g_vectorShipState.headingDegrees > 180.0f)  g_vectorShipState.headingDegrees -= 360.0f;
        while (g_vectorShipState.headingDegrees < -180.0f) g_vectorShipState.headingDegrees += 360.0f;
        ApplyVectorShipHeadingModeLocked(slot, g_vectorShipState);
        dirty = true;
    }
    else {
        // Keep slot heading in sync (e.g. after respawn).
        ApplyVectorShipHeadingModeLocked(slot, g_vectorShipState);
    }

    // --- Thrust: apply along heading (NOT along velocity). ---
    if (g_vectorShipInputState.thrust && deltaSeconds > 0.0f) {
        float thrustImpulse = g_vectorShipState.thrustStep * deltaSeconds;
        slot.velocityX += slot.headingX * thrustImpulse;
        slot.velocityY += slot.headingY * thrustImpulse;
        ClampVectorShipVelocity(slot, g_vectorShipState.maxSpeed);
        dirty = true;
    }
    if (g_vectorShipInputState.reverse && g_vectorShipState.reverseThrustStep > 0.0f && deltaSeconds > 0.0f) {
        float thrustImpulse = g_vectorShipState.reverseThrustStep * deltaSeconds;
        slot.velocityX -= slot.headingX * thrustImpulse;
        slot.velocityY -= slot.headingY * thrustImpulse;
        ClampVectorShipVelocity(slot, g_vectorShipState.maxSpeed);
        dirty = true;
    }

    // --- Drag (Asteroids has a tiny long-tail drag so the ship eventually slows). ---
    if (g_vectorShipState.drag > 0.0f && deltaSeconds > 0.0f) {
        float dragFactor = (std::max)(0.0f, 1.0f - g_vectorShipState.drag * deltaSeconds * 60.0f);
        if (dragFactor < 1.0f && VectorShipLength(slot.velocityX, slot.velocityY) > 0.001f) {
            slot.velocityX *= dragFactor;
            slot.velocityY *= dragFactor;
            dirty = true;
        }
    }

    // --- Invulnerability countdown / steady visibility mode alpha ---
    // These player-icon trial modes are for readability testing, so keep the
    // icon visually steady even while the ship is temporarily invulnerable.
    if (g_vectorShipState.invulnerableRemainingSeconds > 0.0f) {
        g_vectorShipState.invulnerableRemainingSeconds = (std::max)(0.0f, g_vectorShipState.invulnerableRemainingSeconds - deltaSeconds);
        if (g_vectorShipState.invulnerableRemainingSeconds > 0.0f) {
            slot.colorA = BgePlayerIconVisibilityModeAlpha(g_vectorShipState.playerIconVisibilityMode, true, false);
        }
        else {
            slot.colorA = BgePlayerIconVisibilityModeAlpha(g_vectorShipState.playerIconVisibilityMode, false, false);
        }
        dirty = true;
    }
    else if (slot.colorA != BgePlayerIconVisibilityModeAlpha(g_vectorShipState.playerIconVisibilityMode, false, false)) {
        // Defensive: if some other path left the ship dim, restore the active
        // visibility mode's normal alpha.
        slot.colorA = BgePlayerIconVisibilityModeAlpha(g_vectorShipState.playerIconVisibilityMode, false, false);
        dirty = true;
    }

    if (dirty) {
        g_ballVelocityX = slot.velocityX;
        g_ballVelocityY = slot.velocityY;
        g_ballAnimationRunning = true;
        g_rendererStateDirty = true;
        BgeUpdateCollisionFlags(g_objectSlots);
        RefreshSelectedObjectGlobalsLocked();
        PersistActiveObjectGroupLocked();
    }
    return dirty;
}

std::wstring EditModeName(BgeEditMode mode)
{
    switch (mode) {
    case BgeEditMode::Translate:
        return L"Translate";
    case BgeEditMode::Resize:
        return L"Resize";
    case BgeEditMode::Rotate:
        return L"Rotate";
    }
    return L"Translate";
}

float CurrentEditRateMultiplier()
{
    int index = (std::max)(0, (std::min)(g_editRateIndex, static_cast<int>(BGE_EDIT_RATE_MULTIPLIERS.size()) - 1));
    return BGE_EDIT_RATE_MULTIPLIERS[index];
}

std::wstring EditRateLabel()
{
    int index = (std::max)(0, (std::min)(g_editRateIndex, static_cast<int>(BGE_EDIT_RATE_LABELS.size()) - 1));
    return BGE_EDIT_RATE_LABELS[index];
}

void UpdateEditModeStatus()
{
    if (g_editModeStatus) {
        std::wstring label = EditModeName(g_editMode) + L" " + EditRateLabel();
        SetWindowTextW(g_editModeStatus, label.c_str());
    }
}

bool SetEditModeFromText(const std::wstring& modeText)
{
    std::wstring lower = LowerArg(modeText);
    if (lower == L"translate" || lower == L"move") {
        g_editMode = BgeEditMode::Translate;
    }
    else if (lower == L"resize" || lower == L"scale") {
        g_editMode = BgeEditMode::Resize;
    }
    else if (lower == L"rotate" || lower == L"turn") {
        g_editMode = BgeEditMode::Rotate;
    }
    else {
        return false;
    }

    UpdateEditModeStatus();
    RefreshMappingWindow();
    return true;
}

void AdjustEditRate(int direction)
{
    g_editRateIndex = (std::max)(0, (std::min)(g_editRateIndex + direction, static_cast<int>(BGE_EDIT_RATE_MULTIPLIERS.size()) - 1));
    UpdateEditModeStatus();
    RefreshMappingWindow();
    SetCommandStatus(L"Rate: " + EditRateLabel());
}

bool SetEditRateFromText(const std::wstring& rateText)
{
    std::wstring lower = LowerArg(rateText);
    if (lower == L"up" || lower == L"faster" || lower == L"right") {
        AdjustEditRate(1);
        return true;
    }
    if (lower == L"down" || lower == L"slower" || lower == L"left") {
        AdjustEditRate(-1);
        return true;
    }

    if (!lower.empty() && lower.back() == L'x') {
        lower.pop_back();
    }

    float requestedRate = 0.0f;
    if (!TryParseFloatArg(lower, requestedRate)) {
        return false;
    }

    for (size_t index = 0; index < BGE_EDIT_RATE_MULTIPLIERS.size(); ++index) {
        if (std::fabs(BGE_EDIT_RATE_MULTIPLIERS[index] - requestedRate) < 0.001f) {
            g_editRateIndex = static_cast<int>(index);
            UpdateEditModeStatus();
            RefreshMappingWindow();
            SetCommandStatus(L"Rate: " + EditRateLabel());
            return true;
        }
    }

    return false;
}

void CycleEditMode(int direction)
{
    int modeIndex = 0;
    if (g_editMode == BgeEditMode::Resize) {
        modeIndex = 1;
    }
    else if (g_editMode == BgeEditMode::Rotate) {
        modeIndex = 2;
    }
    modeIndex = (modeIndex + direction + 3) % 3;
    g_editMode = modeIndex == 0 ? BgeEditMode::Translate : (modeIndex == 1 ? BgeEditMode::Resize : BgeEditMode::Rotate);
    UpdateEditModeStatus();
    RefreshMappingWindow();
    SetCommandStatus(L"Mode: " + EditModeName(g_editMode));
}

bool TranslateSelectedObject(float deltaX, float deltaY, std::wstring& statusText)
{
    int selectedSlot = 0;
    float x = 0.0f;
    float y = 0.0f;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        BgeObjectSlotState& slot = g_objectSlots[g_selectedObjectSlot];
        if (!g_objectSelectionActive || !slot.visible || slot.isDeleted) {
            statusText = L"Select a visible object";
            return false;
        }
        slot.x += deltaX;
        slot.y += deltaY;
        ClampObjectToClientLocked(slot);
        selectedSlot = g_selectedObjectSlot;
        x = slot.x;
        y = slot.y;
        g_rendererStateDirty = true;
        PersistActiveObjectGroupLocked();
    }
    SyncBallControls();
    InvalidateRect(g_hWnd, nullptr, FALSE);
    std::ostringstream message;
    message << "[BouncingBallEdit] translate slot=" << (selectedSlot + 1) << " x=" << x << " y=" << y;
    LogRendererMessage(message.str());
    statusText = L"Object " + std::to_wstring(selectedSlot + 1) + L" translated";
    return true;
}

bool ResizeSelectedObject(float deltaRadius, std::wstring& statusText)
{
    int selectedSlot = 0;
    float radius = 0.0f;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        BgeObjectSlotState& slot = g_objectSlots[g_selectedObjectSlot];
        if (!g_objectSelectionActive || !slot.visible || slot.isDeleted) {
            statusText = L"Select a visible object";
            return false;
        }
        slot.radius = ClampFloat(slot.radius + deltaRadius, BGE_MIN_OBJECT_RADIUS, BGE_MAX_OBJECT_RADIUS);
        ClampObjectToClientLocked(slot);
        selectedSlot = g_selectedObjectSlot;
        radius = slot.radius;
        g_rendererStateDirty = true;
        PersistActiveObjectGroupLocked();
    }
    SyncBallControls();
    InvalidateRect(g_hWnd, nullptr, FALSE);
    std::ostringstream message;
    message << "[BouncingBallEdit] resize slot=" << (selectedSlot + 1) << " radius=" << radius;
    LogRendererMessage(message.str());
    statusText = L"Object " + std::to_wstring(selectedSlot + 1) + L" resized";
    return true;
}

bool ApplySelectedObjectVectorIntent(float deltaDegrees, float deltaMagnitude, const std::wstring& actionName, std::wstring& statusText)
{
    int selectedSlot = 0;
    float magnitude = 0.0f;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        BgeObjectSlotState& slot = g_objectSlots[g_selectedObjectSlot];
        if (!g_objectSelectionActive || !slot.visible || slot.isDeleted) {
            statusText = L"Select a visible object";
            return false;
        }

        float currentMagnitude = std::sqrt(slot.velocityX * slot.velocityX + slot.velocityY * slot.velocityY);
        if (currentMagnitude < 1.0f) {
            currentMagnitude = 180.0f;
            slot.velocityX = currentMagnitude;
            slot.velocityY = 0.0f;
        }

        float radians = std::atan2(slot.velocityY, slot.velocityX) + deltaDegrees * 3.14159265358979323846f / 180.0f;
        magnitude = (std::max)(0.0f, currentMagnitude + deltaMagnitude);
        slot.velocityX = std::cos(radians) * magnitude;
        slot.velocityY = std::sin(radians) * magnitude;
        g_ballVelocityX = slot.velocityX;
        g_ballVelocityY = slot.velocityY;
        selectedSlot = g_selectedObjectSlot;
        velocityX = slot.velocityX;
        velocityY = slot.velocityY;
        g_rendererStateDirty = true;
        PersistActiveObjectGroupLocked();
    }
    SyncBallControls();
    InvalidateRect(g_hWnd, nullptr, FALSE);
    const char* logAction = deltaMagnitude != 0.0f ? "magnitude" : "rotate";
    std::ostringstream message;
    message << "[BouncingBallEdit] vector-intent slot=" << (selectedSlot + 1) << " action=" << logAction
        << " degrees=" << deltaDegrees << " delta-magnitude=" << deltaMagnitude << " magnitude=" << magnitude
        << " vx=" << velocityX << " vy=" << velocityY;
    LogRendererMessage(message.str());
    if (deltaMagnitude != 0.0f && deltaDegrees == 0.0f) {
        statusText = L"Object " + std::to_wstring(selectedSlot + 1) + L" magnitude " + std::to_wstring(static_cast<int>(magnitude));
    }
    else {
        statusText = L"Object " + std::to_wstring(selectedSlot + 1) + L" " + actionName;
    }
    return true;
}

bool RotateSelectedObject(float deltaDegrees, std::wstring& statusText)
{
    return ApplySelectedObjectVectorIntent(deltaDegrees, 0.0f, L"rotated", statusText);
}

bool AdjustSelectedObjectVectorMagnitude(float deltaMagnitude, std::wstring& statusText)
{
    return ApplySelectedObjectVectorIntent(0.0f, deltaMagnitude, L"magnitude", statusText);
}

std::wstring LowerArg(std::wstring value)
{
    for (wchar_t& ch : value) {
        ch = static_cast<wchar_t>(std::towlower(ch));
    }
    return value;
}

const BgePluginDescriptor* FindBgePluginDescriptor(const std::wstring& pluginId)
{
    std::wstring requested = LowerArg(pluginId);
    for (const auto& descriptor : kBgePluginRegistry) {
        if (LowerArg(descriptor.id) == requested) {
            return &descriptor;
        }
    }
    return nullptr;
}

bool BgePluginAlreadyImported(const std::wstring& pluginId)
{
    std::wstring requested = LowerArg(pluginId);
    return std::any_of(g_importedBgePlugins.begin(), g_importedBgePlugins.end(), [&requested](const std::wstring& imported) {
        return LowerArg(imported) == requested;
    });
}

void ImportBgePluginDescriptor(const BgePluginDescriptor& descriptor)
{
    if (!BgePluginAlreadyImported(descriptor.id)) {
        g_importedBgePlugins.push_back(descriptor.id);
    }
}

std::wstring BgePluginDescriptorLine(const BgePluginDescriptor& descriptor)
{
    return std::wstring(descriptor.id) + L" [" + descriptor.kind + L"] " + descriptor.command + L" " + descriptor.flags;
}

std::wstring BgePluginListText()
{
    std::wstring text = L"BGE plugins:";
    for (const auto& descriptor : kBgePluginRegistry) {
        text += L"\r\n  " + BgePluginDescriptorLine(descriptor);
    }
    return text;
}

std::wstring BgePluginCommandSignatureText()
{
    std::wstring text = L"BGE plugin command signatures:";
    for (const auto& descriptor : kBgePluginRegistry) {
        text += L"\r\n  " + std::wstring(descriptor.command) + L" " + descriptor.flags + L" <= " + descriptor.id;
    }
    return text;
}

std::wstring BgeImportedPluginText()
{
    if (g_importedBgePlugins.empty()) {
        return L"Imported plugins: none";
    }

    std::wstring text = L"Imported plugins:";
    for (const auto& pluginId : g_importedBgePlugins) {
        text += L" " + pluginId;
    }
    return text;
}

std::wstring BgePluginExplainText(const BgePluginDescriptor& descriptor)
{
    std::wstring text = L"Plugin: " + std::wstring(descriptor.id);
    text += L"\r\nKind: " + std::wstring(descriptor.kind);
    text += L"\r\nSummary: " + std::wstring(descriptor.summary);
    text += L"\r\nCommand: " + std::wstring(descriptor.command);
    text += L"\r\nFlags: " + std::wstring(descriptor.flags);
    text += L"\r\nEmits: " + std::wstring(descriptor.emits);
    return text;
}

void AddBgePluginRegistryAttributesToOpNode(const std::shared_ptr<OpNode>& root)
{
    if (!root) {
        return;
    }

    root->SetAttribute("plugin.registry", "enabled");
    root->SetAttribute("plugin.registry.source", "bge.static.command-signatures");
    for (const auto& descriptor : kBgePluginRegistry) {
        std::string key = std::string("plugin.") + Narrow(descriptor.id);
        root->SetAttribute(key, "available");
        root->SetAttribute(key + ".kind", Narrow(descriptor.kind));
        root->SetAttribute(key + ".command", Narrow(descriptor.command));
        root->SetAttribute(key + ".flags", Narrow(descriptor.flags));
        root->SetAttribute(key + ".emits", Narrow(descriptor.emits));
    }
}

bool ExecuteBgePluginCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"list";

    if (subcommand == L"list" || subcommand == L"status") {
        statusText = BgePluginListText() + L"\r\n" + BgeImportedPluginText();
        return true;
    }

    if (subcommand == L"commands" || subcommand == L"signatures") {
        statusText = BgePluginCommandSignatureText();
        return true;
    }

    if (subcommand == L"import" || subcommand == L"require" || subcommand == L"use" || subcommand == L"load") {
        if (tokens.size() < 3) {
            statusText = L"Use: plugin import <plugin-id>";
            return false;
        }

        const BgePluginDescriptor* descriptor = FindBgePluginDescriptor(tokens[2]);
        if (!descriptor) {
            statusText = L"BGE plugin not found: " + tokens[2];
            return false;
        }

        ImportBgePluginDescriptor(*descriptor);

        statusText = L"Plugin imported: " + BgePluginDescriptorLine(*descriptor);
        return true;
    }

    if (subcommand == L"import-set" || subcommand == L"require-set" || subcommand == L"import-bundle" || subcommand == L"require-bundle" || subcommand == L"bundle" || subcommand == L"load-set") {
        if (tokens.size() < 3) {
            statusText = L"Use: plugin import-set asteroids";
            return false;
        }

        std::wstring setName = LowerArg(tokens[2]);
        if (setName != L"asteroids" && setName != L"space-rocks") {
            statusText = L"BGE plugin set not found: " + tokens[2];
            return false;
        }

        for (const auto* pluginId : kBgeAsteroidsPluginSet) {
            const BgePluginDescriptor* descriptor = FindBgePluginDescriptor(pluginId);
            if (descriptor) {
                ImportBgePluginDescriptor(*descriptor);
            }
        }

        statusText = L"Plugin set imported: asteroids -> " + BgeImportedPluginText();
        return true;
    }

    if (subcommand == L"explain" || subcommand == L"show") {
        if (tokens.size() < 3) {
            statusText = L"Use: plugin explain <plugin-id>";
            return false;
        }

        const BgePluginDescriptor* descriptor = FindBgePluginDescriptor(tokens[2]);
        if (!descriptor) {
            statusText = L"BGE plugin not found: " + tokens[2];
            return false;
        }

        statusText = BgePluginExplainText(*descriptor);
        return true;
    }

    statusText = L"Use: plugin list | plugin import <plugin-id> | plugin import-set asteroids | plugin explain <plugin-id> | plugin commands";
    return false;
}

bool ExecuteBgeInspectCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"commands";
    if (subcommand == L"commands" || subcommand == L"plugins" || subcommand == L"signatures") {
        statusText = BgePluginCommandSignatureText();
        return true;
    }

    statusText = L"Use: inspect commands";
    return false;
}

bool TryParseFloatArg(const std::wstring& text, float& value)
{
    wchar_t* parseEnd = nullptr;
    value = static_cast<float>(std::wcstod(text.c_str(), &parseEnd));
    return parseEnd != text.c_str() && *parseEnd == L'\0';
}

bool TryParseSlotArg(const std::wstring& text, int& slotIndex)
{
    float value = 0.0f;
    if (!TryParseFloatArg(text, value)) {
        return false;
    }
    int oneBasedSlot = static_cast<int>(value);
    if (oneBasedSlot < 1 || oneBasedSlot > BGE_OBJECT_SLOT_COUNT) {
        return false;
    }
    slotIndex = oneBasedSlot - 1;
    return true;
}

float NormalizeColorArg(float value)
{
    value = (std::max)(0.0f, value);
    if (value > 1.0f) {
        value /= 255.0f;
    }
    return (std::min)(1.0f, value);
}

bool TrySetRendererApiFromArg(const std::wstring& value)
{
    std::wstring lower = LowerArg(value);
    if (lower == L"dx12" || lower == L"directx12" || lower == L"12") {
        g_rendererApi.store(BgeRendererApi::DirectX12);
        g_rendererSwitchRequested = true;
        g_rendererStateDirty = true;
        return true;
    }
    if (lower == L"dx11" || lower == L"directx11" || lower == L"11") {
        g_rendererApi.store(BgeRendererApi::DirectX11);
        g_rendererSwitchRequested = true;
        g_rendererStateDirty = true;
        return true;
    }
    return false;
}

std::wstring RendererApiArg()
{
    return g_rendererApi.load() == BgeRendererApi::DirectX12 ? L"dx12" : L"dx11";
}

void AddWorkerCliArg(const std::wstring& value)
{
    g_workerCliArgs.push_back(value);
}

void ApplySessionName(const std::wstring& sessionName)
{
    std::wstring cleanName;
    for (wchar_t ch : sessionName) {
        if ((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') || ch == L'_' || ch == L'-') {
            cleanName += ch;
        }
    }
    if (cleanName.empty()) {
        return;
    }

    g_bgeSharedMemoryName = L"Local\\BasicGameEngineSharedMemory_" + cleanName;
    g_bgeCoordMutexName = L"Local\\BasicGameEngineCoordMutex_" + cleanName;
    g_sessionCliName = cleanName;
}

bool ReadNextArg(LPWSTR* argv, int argc, int& index, std::wstring& value)
{
    if (index + 1 >= argc) {
        return false;
    }
    value = argv[++index];
    return true;
}

bool HasEmbeddedGameScriptResource()
{
    return FindResourceW(nullptr, kBgeEmbeddedGameScriptResourceName, RT_RCDATA) != nullptr;
}

void ParseRuntimeArgs()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return;
    }

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--player" || arg == L"--game-only" || arg == L"--arcade-runtime") {
            g_playerRuntimeMode = true;
        }
        else if ((arg == L"--game-title" || arg == L"--player-title") && i + 1 < argc) {
            g_playerRuntimeTitle = argv[++i];
        }
        else if (arg.rfind(L"--game-title=", 0) == 0) {
            g_playerRuntimeTitle = arg.substr(13);
        }
        else if (arg.rfind(L"--player-title=", 0) == 0) {
            g_playerRuntimeTitle = arg.substr(15);
        }
        else if ((arg == L"--worker" || arg == L"--role") && i + 1 < argc) {
            g_workerName = argv[++i];
            g_workerRoleRequested = true;
        }
        else if (arg.rfind(L"--worker=", 0) == 0) {
            g_workerName = arg.substr(9);
            g_workerRoleRequested = true;
        }
        else if (arg.rfind(L"--role=", 0) == 0) {
            g_workerName = arg.substr(7);
            g_workerRoleRequested = true;
        }
        else if (arg == L"--name" && i + 1 < argc) {
            g_workerName = argv[++i];
        }
        else if (arg.rfind(L"--name=", 0) == 0) {
            g_workerName = arg.substr(7);
        }
        else if (arg == L"--launch-basic-game") {
            g_launchBasicGameStack = true;
        }
        else if (arg == L"--session" && i + 1 < argc) {
            std::wstring value = argv[++i];
            ApplySessionName(value);
            AddWorkerCliArg(L"--session");
            AddWorkerCliArg(value);
        }
        else if (arg.rfind(L"--session=", 0) == 0) {
            std::wstring value = arg.substr(10);
            ApplySessionName(value);
            AddWorkerCliArg(L"--session");
            AddWorkerCliArg(value);
        }
        else if ((arg == L"--launch-worker" || arg == L"--launch") && i + 1 < argc) {
            g_launchRoles.push_back(argv[++i]);
        }
        else if (arg.rfind(L"--launch-worker=", 0) == 0) {
            g_launchRoles.push_back(arg.substr(16));
        }
        else if (arg.rfind(L"--launch=", 0) == 0) {
            g_launchRoles.push_back(arg.substr(9));
        }
        else if ((arg == L"--ui-target" || arg == L"--ui-group") && i + 1 < argc) {
            g_cliControllerTarget = argv[++i];
        }
        else if (arg.rfind(L"--ui-target=", 0) == 0) {
            g_cliControllerTarget = arg.substr(12);
        }
        else if (arg.rfind(L"--ui-group=", 0) == 0) {
            g_cliControllerTarget = arg.substr(11);
        }
        else if ((arg == L"--inspect-worker" || arg == L"--inspect-group" || arg == L"--inspect-ui" || arg == L"--show-worker") && i + 1 < argc) {
            g_cliInspectSelectors.push_back(argv[++i]);
        }
        else if (arg.rfind(L"--inspect-worker=", 0) == 0) {
            g_cliInspectSelectors.push_back(arg.substr(17));
        }
        else if (arg.rfind(L"--inspect-group=", 0) == 0) {
            g_cliInspectSelectors.push_back(arg.substr(16));
        }
        else if (arg.rfind(L"--inspect-ui=", 0) == 0) {
            g_cliInspectSelectors.push_back(arg.substr(13));
        }
        else if (arg.rfind(L"--show-worker=", 0) == 0) {
            g_cliInspectSelectors.push_back(arg.substr(14));
        }
        else if ((arg == L"--hide-worker" || arg == L"--hide-group") && i + 1 < argc) {
            g_cliHideSelectors.push_back(argv[++i]);
        }
        else if (arg.rfind(L"--hide-worker=", 0) == 0) {
            g_cliHideSelectors.push_back(arg.substr(14));
        }
        else if (arg.rfind(L"--hide-group=", 0) == 0) {
            g_cliHideSelectors.push_back(arg.substr(13));
        }
        else if (arg == L"--hide-workers" || arg == L"--headless-workers") {
            g_hideWorkerWindowsByDefault = true;
        }
        else if (arg == L"--show-workers") {
            g_hideWorkerWindowsByDefault = false;
        }
        else if ((arg == L"--ui-command" || arg == L"--controller-command") && i + 1 < argc) {
            g_pendingControllerCommands.push_back(argv[++i]);
        }
        else if (arg.rfind(L"--ui-command=", 0) == 0) {
            g_pendingControllerCommands.push_back(arg.substr(13));
        }
        else if (arg.rfind(L"--controller-command=", 0) == 0) {
            g_pendingControllerCommands.push_back(arg.substr(21));
        }
        else if ((arg == L"--game-file" || arg == L"--construction-file" || arg == L"--command-file") && i + 1 < argc) {
            g_cliConstructionArtifactPaths.push_back(argv[++i]);
        }
        else if ((arg == L"--game-script" || arg == L"--commands") && i + 1 < argc) {
            g_playerRuntimeMode = true;
            g_cliConstructionArtifactPaths.push_back(argv[++i]);
        }
        else if (arg.rfind(L"--game-file=", 0) == 0) {
            g_cliConstructionArtifactPaths.push_back(arg.substr(12));
        }
        else if (arg.rfind(L"--game-script=", 0) == 0) {
            g_playerRuntimeMode = true;
            g_cliConstructionArtifactPaths.push_back(arg.substr(14));
        }
        else if (arg.rfind(L"--commands=", 0) == 0) {
            g_playerRuntimeMode = true;
            g_cliConstructionArtifactPaths.push_back(arg.substr(11));
        }
        else if (arg.rfind(L"--construction-file=", 0) == 0) {
            g_cliConstructionArtifactPaths.push_back(arg.substr(20));
        }
        else if (arg.rfind(L"--command-file=", 0) == 0) {
            g_cliConstructionArtifactPaths.push_back(arg.substr(15));
        }
        else if (arg == L"--background" && i + 1 < argc) {
            g_backgroundImagePath = argv[++i];
            g_backgroundImageDirty = true;
        }
        else if (arg.rfind(L"--background=", 0) == 0) {
            g_backgroundImagePath = arg.substr(13);
            g_backgroundImageDirty = true;
        }
        else if ((arg == L"--renderer" || arg == L"--render-api") && i + 1 < argc) {
            std::wstring value = argv[++i];
            if (TrySetRendererApiFromArg(value)) {
                AddWorkerCliArg(L"--renderer");
                AddWorkerCliArg(RendererApiArg());
            }
        }
        else if (arg.rfind(L"--renderer=", 0) == 0) {
            if (TrySetRendererApiFromArg(arg.substr(11))) {
                AddWorkerCliArg(L"--renderer");
                AddWorkerCliArg(RendererApiArg());
            }
        }
        else if (arg.rfind(L"--render-api=", 0) == 0) {
            if (TrySetRendererApiFromArg(arg.substr(13))) {
                AddWorkerCliArg(L"--renderer");
                AddWorkerCliArg(RendererApiArg());
            }
        }
        else if ((arg == L"--select-object" || arg == L"--select-vector") && i + 1 < argc) {
            int slotIndex = 0;
            std::wstring value = argv[++i];
            if (TryParseSlotArg(value, slotIndex)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                SelectObjectSlotStateLocked(slotIndex);
                AddWorkerCliArg(arg);
                AddWorkerCliArg(std::to_wstring(slotIndex + 1));
            }
        }
        else if (arg.rfind(L"--select-object=", 0) == 0 || arg.rfind(L"--select-vector=", 0) == 0) {
            size_t eq = arg.find(L'=');
            int slotIndex = 0;
            if (TryParseSlotArg(arg.substr(eq + 1), slotIndex)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                SelectObjectSlotStateLocked(slotIndex);
                AddWorkerCliArg(arg.substr(0, eq));
                AddWorkerCliArg(std::to_wstring(slotIndex + 1));
            }
        }
        else if ((arg == L"--add-object" || arg == L"--object") && i + 1 < argc) {
            int slotIndex = 0;
            std::wstring value = argv[++i];
            if (TryParseSlotArg(value, slotIndex)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                AddObjectSlotStateLocked(slotIndex);
                AddWorkerCliArg(L"--add-object");
                AddWorkerCliArg(std::to_wstring(slotIndex + 1));
            }
        }
        else if (arg.rfind(L"--add-object=", 0) == 0 || arg.rfind(L"--object=", 0) == 0) {
            size_t eq = arg.find(L'=');
            int slotIndex = 0;
            if (TryParseSlotArg(arg.substr(eq + 1), slotIndex)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                AddObjectSlotStateLocked(slotIndex);
                AddWorkerCliArg(L"--add-object");
                AddWorkerCliArg(std::to_wstring(slotIndex + 1));
            }
        }
        else if (arg == L"--add-ball") {
            int selectedSlot = 0;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                AddObjectSlotStateLocked(-1);
                selectedSlot = g_selectedObjectSlot;
            }
            AddWorkerCliArg(L"--add-object");
            AddWorkerCliArg(std::to_wstring(selectedSlot + 1));
        }
        else if ((arg == L"--vector" || arg == L"--velocity") && i + 2 < argc) {
            float velocityX = 0.0f;
            float velocityY = 0.0f;
            std::wstring xArg = argv[++i];
            std::wstring yArg = argv[++i];
            if (TryParseFloatArg(xArg, velocityX) && TryParseFloatArg(yArg, velocityY)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                ApplyObjectVectorStateLocked(velocityX, velocityY);
                AddWorkerCliArg(L"--vector");
                AddWorkerCliArg(xArg);
                AddWorkerCliArg(yArg);
            }
        }
        else if (arg.rfind(L"--vector=", 0) == 0 || arg.rfind(L"--velocity=", 0) == 0) {
            std::wstring value = arg.substr(arg.find(L'=') + 1);
            size_t comma = value.find(L',');
            float velocityX = 0.0f;
            float velocityY = 0.0f;
            if (comma != std::wstring::npos && TryParseFloatArg(value.substr(0, comma), velocityX) && TryParseFloatArg(value.substr(comma + 1), velocityY)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                ApplyObjectVectorStateLocked(velocityX, velocityY);
                AddWorkerCliArg(L"--vector");
                AddWorkerCliArg(value.substr(0, comma));
                AddWorkerCliArg(value.substr(comma + 1));
            }
        }
        else if (arg == L"--vx" && i + 1 < argc) {
            float velocityX = 0.0f;
            std::wstring value = argv[++i];
            if (TryParseFloatArg(value, velocityX)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                ApplyObjectVectorStateLocked(velocityX, g_objectSlots[g_selectedObjectSlot].velocityY);
                AddWorkerCliArg(L"--vector");
                AddWorkerCliArg(value);
                AddWorkerCliArg(std::to_wstring(g_objectSlots[g_selectedObjectSlot].velocityY));
            }
        }
        else if (arg == L"--vy" && i + 1 < argc) {
            float velocityY = 0.0f;
            std::wstring value = argv[++i];
            if (TryParseFloatArg(value, velocityY)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                ApplyObjectVectorStateLocked(g_objectSlots[g_selectedObjectSlot].velocityX, velocityY);
                AddWorkerCliArg(L"--vector");
                AddWorkerCliArg(std::to_wstring(g_objectSlots[g_selectedObjectSlot].velocityX));
                AddWorkerCliArg(value);
            }
        }
        else if ((arg == L"--color" || arg == L"--rgb") && i + 3 < argc) {
            float colorR = 0.0f;
            float colorG = 0.0f;
            float colorB = 0.0f;
            std::wstring rArg = argv[++i];
            std::wstring gArg = argv[++i];
            std::wstring bArg = argv[++i];
            if (TryParseFloatArg(rArg, colorR) && TryParseFloatArg(gArg, colorG) && TryParseFloatArg(bArg, colorB)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                ApplyObjectColorStateLocked(NormalizeColorArg(colorR), NormalizeColorArg(colorG), NormalizeColorArg(colorB));
                AddWorkerCliArg(L"--color");
                AddWorkerCliArg(rArg);
                AddWorkerCliArg(gArg);
                AddWorkerCliArg(bArg);
            }
        }
        else if (arg.rfind(L"--color=", 0) == 0 || arg.rfind(L"--rgb=", 0) == 0) {
            std::wstring value = arg.substr(arg.find(L'=') + 1);
            size_t firstComma = value.find(L',');
            size_t secondComma = firstComma == std::wstring::npos ? std::wstring::npos : value.find(L',', firstComma + 1);
            float colorR = 0.0f;
            float colorG = 0.0f;
            float colorB = 0.0f;
            if (firstComma != std::wstring::npos && secondComma != std::wstring::npos &&
                TryParseFloatArg(value.substr(0, firstComma), colorR) &&
                TryParseFloatArg(value.substr(firstComma + 1, secondComma - firstComma - 1), colorG) &&
                TryParseFloatArg(value.substr(secondComma + 1), colorB)) {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                ApplyObjectColorStateLocked(NormalizeColorArg(colorR), NormalizeColorArg(colorG), NormalizeColorArg(colorB));
                AddWorkerCliArg(L"--color");
                AddWorkerCliArg(value.substr(0, firstComma));
                AddWorkerCliArg(value.substr(firstComma + 1, secondComma - firstComma - 1));
                AddWorkerCliArg(value.substr(secondComma + 1));
            }
        }
        else if (arg == L"--start" || arg == L"--start-animation") {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            if (!AnyObjectSlotVisibleLocked()) {
                AddObjectSlotStateLocked(g_selectedObjectSlot);
            }
            g_ballAnimationRunning = true;
            g_rendererStateDirty = true;
            AddWorkerCliArg(L"--start-animation");
        }
        else if ((arg == L"--sound-slot" || arg == L"--select-sound") && i + 1 < argc) {
            int slotIndex = 0;
            std::wstring value = argv[++i];
            if (TryParseSlotArg(value, slotIndex)) {
                g_activeSoundSlot = slotIndex;
                AddWorkerCliArg(L"--sound-slot");
                AddWorkerCliArg(std::to_wstring(slotIndex + 1));
            }
        }
        else if (arg.rfind(L"--sound-slot=", 0) == 0 || arg.rfind(L"--select-sound=", 0) == 0) {
            int slotIndex = 0;
            if (TryParseSlotArg(arg.substr(arg.find(L'=') + 1), slotIndex)) {
                g_activeSoundSlot = slotIndex;
                AddWorkerCliArg(L"--sound-slot");
                AddWorkerCliArg(std::to_wstring(slotIndex + 1));
            }
        }
    }

    if (!g_playerRuntimeMode && argc == 1 && HasEmbeddedGameScriptResource()) {
        g_playerRuntimeMode = true;
        if (g_playerRuntimeTitle.empty()) {
            g_playerRuntimeTitle = std::filesystem::path(argv[0]).stem().wstring();
        }
    }

    if (g_playerRuntimeMode) {
        g_workerName = L"bge.game-loop";
        g_workerRoleRequested = true;
        g_launchBasicGameStack = false;
        BgeSetRenderTopInset(0.0f);
    }
    else {
        BgeSetRenderTopInset(BGE_RENDER_TOP_INSET);
    }

    LocalFree(argv);
}

class CoordLock {
public:
    explicit CoordLock(HANDLE handle) : handle_(handle), locked_(false) {
        if (handle_) {
            DWORD result = WaitForSingleObject(handle_, 5000);
            locked_ = (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED);
        }
    }

    ~CoordLock() {
        if (locked_) {
            ReleaseMutex(handle_);
        }
    }

    bool locked() const { return locked_; }

private:
    HANDLE handle_;
    bool locked_;
};

std::string Narrow(const std::wstring& value)
{
    if (value.empty()) {
        return std::string();
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return std::string();
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    result.pop_back();
    return result;
}

std::wstring WidenUtf8(const std::string& value)
{
    if (value.empty()) {
        return std::wstring();
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        size = MultiByteToWideChar(CP_ACP, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
        if (size <= 0) {
            return std::wstring();
        }
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size);
        return result;
    }

    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::wstring ExePath()
{
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        return L"";
    }
    return exePath;
}

std::wstring QuoteArg(const std::wstring& value)
{
    std::wstring escaped = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'\"') {
            escaped += L"\\\"";
        }
        else {
            escaped += ch;
        }
    }
    escaped += L"\"";
    return escaped;
}

std::wstring CommandFileArg(const std::wstring& value)
{
    if (value.empty()) {
        return L"\"\"";
    }
    if (value.find_first_of(L" \t\r\n\"") != std::wstring::npos) {
        return QuoteArg(value);
    }
    return value;
}

void RecordWorkerCommandHistory(const std::wstring& commandText)
{
    if (g_isController || g_playerRuntimeMode) {
        return;
    }

    std::vector<std::wstring> tokens = TokenizeCommandText(commandText);
    if (!tokens.empty() && LowerArg(tokens[0]) == L"export") {
        return;
    }

    g_workerCommandHistory.push_back(commandText);
    if (g_workerCommandHistory.size() > BGE_CONTROLLER_HISTORY_LIMIT) {
        g_workerCommandHistory.erase(g_workerCommandHistory.begin());
    }
}

std::wstring ControllerBaseCli()
{
    std::wstring command = QuoteArg(ExePath());
    if (!g_sessionCliName.empty()) {
        command += L" --session " + QuoteArg(g_sessionCliName);
    }
    return command;
}

std::wstring ControllerCommandDetail(const std::wstring& commandText)
{
    return L"Controller command:\r\n  " + commandText + L"\r\n\r\nCLI equivalent:\r\n  " + ControllerBaseCli() + L" --launch-basic-game --ui-command " + QuoteArg(commandText);
}

std::wstring WorkerCommandDetail(const std::wstring& role, const std::wstring& commandText)
{
    int artifactIndex = ArtifactIndexForRole(role);
    std::wstring group = artifactIndex >= 0 ? kControllerArtifacts[artifactIndex].group : RoleShortName(role);
    std::wstring prefixedCommand = RoleShortName(role) + L": " + commandText;
    return L"Target worker:\r\n  " + role + L"\r\n\r\nController command:\r\n  " + prefixedCommand + L"\r\n\r\nCLI equivalent:\r\n  " + ControllerBaseCli() + L" --launch-basic-game --ui-target " + QuoteArg(group) + L" --ui-command " + QuoteArg(prefixedCommand);
}

std::wstring TargetCommandDetail(const BgeControllerArtifactSpec& spec)
{
    std::wstring controllerCommand = std::wstring(L"target ") + QuoteArg(spec.group) + L"\r\ninspect " + QuoteArg(spec.displayName);
    return L"Controller commands:\r\n  " + controllerCommand + L"\r\n\r\nCLI target only:\r\n  " + ControllerBaseCli() + L" --launch-basic-game --ui-target " + QuoteArg(spec.group) + L"\r\n\r\nCLI target and open window:\r\n  " + ControllerBaseCli() + L" --launch-basic-game --ui-target " + QuoteArg(spec.group) + L" --inspect-worker " + QuoteArg(spec.role);
}

std::wstring LaunchCommandDetail(const std::wstring& role)
{
    return L"Controller command:\r\n  launch " + QuoteArg(RoleShortName(role)) + L"\r\n\r\nCLI equivalent:\r\n  " + ControllerBaseCli() + L" --launch-worker " + QuoteArg(role);
}

std::wstring InspectCommandDetail(const std::wstring& role)
{
    return L"Controller command:\r\n  inspect " + QuoteArg(RoleShortName(role)) + L"\r\n\r\nCLI equivalent:\r\n  " + ControllerBaseCli() + L" --launch-basic-game --inspect-worker " + QuoteArg(role);
}

std::wstring HideCommandDetail(const std::wstring& role)
{
    return L"Controller command:\r\n  hide " + QuoteArg(RoleShortName(role)) + L"\r\n\r\nCLI equivalent:\r\n  " + ControllerBaseCli() + L" --launch-basic-game --hide-worker " + QuoteArg(role);
}

bool WorkerRoleIsRunning(const std::wstring& role)
{
    if (!g_sharedData || !EnsureCoordMutex()) {
        return false;
    }

    CoordLock lock(g_coordMutex);
    if (!lock.locked()) {
        return false;
    }

    PruneStaleWorkersLocked(g_sharedData);
    for (LONG i = 0; i < BGE_MAX_WORKERS; ++i) {
        if (g_sharedData->workers[i].pid != 0 && role == g_sharedData->workers[i].name) {
            return true;
        }
    }
    return false;
}

void LaunchWorkerRole(const std::wstring& role)
{
    if (!g_isController || role.empty()) {
        return;
    }

    if (WorkerRoleIsRunning(role)) {
        AddControllerHistory(L"Launch skipped, already running -> " + role, LaunchCommandDetail(role));
        return;
    }

    std::wstring exe = ExePath();
    if (exe.empty()) {
        AddControllerHistory(L"Launch failed, exe not found -> " + role, LaunchCommandDetail(role));
        return;
    }

    std::wstring commandLine = QuoteArg(exe) + L" --worker " + QuoteArg(role);
    if (!g_backgroundImagePath.empty()) {
        commandLine += L" --background " + QuoteArg(g_backgroundImagePath);
    }
    for (const auto& workerArg : g_workerCliArgs) {
        commandLine += L" " + QuoteArg(workerArg);
    }
    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(exe.c_str(), buffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        AddControllerHistory(L"Launch -> " + role, LaunchCommandDetail(role));
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    else {
        AddControllerHistory(L"Launch failed -> " + role, LaunchCommandDetail(role));
    }
}

void LaunchBasicGameStack()
{
    LaunchWorkerRole(L"bge.game-loop");
    LaunchWorkerRole(L"bge.scene-3d");
    LaunchWorkerRole(L"bge.images");
    LaunchWorkerRole(L"bge.sound");
    LaunchWorkerRole(L"bge.sample-game-one");
    LaunchWorkerRole(L"bge.sample-game-two");
}

void AddControllerMenu(HWND hWnd)
{
    if (!g_isController) {
        return;
    }

    HMENU mainMenu = GetMenu(hWnd);
    if (!mainMenu) {
        return;
    }

    HMENU workerMenu = CreatePopupMenu();
    if (!workerMenu) {
        return;
    }

    AppendMenuW(workerMenu, MF_STRING, IDM_BGE_LAUNCH_STACK, L"Launch Basic Game Stack");
    AppendMenuW(workerMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(workerMenu, MF_STRING, IDM_BGE_LAUNCH_GAME_LOOP, L"Launch Game Loop Worker");
    AppendMenuW(workerMenu, MF_STRING, IDM_BGE_LAUNCH_SCENE_3D, L"Launch 3D Scene Worker");
    AppendMenuW(workerMenu, MF_STRING, IDM_BGE_LAUNCH_IMAGES, L"Launch Image Assets Worker");
    AppendMenuW(workerMenu, MF_STRING, IDM_BGE_LAUNCH_SOUND, L"Launch Sound Worker");
    AppendMenuW(workerMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(workerMenu, MF_STRING, IDM_BGE_LAUNCH_SAMPLE_ONE, L"Launch Sample Game One");
    AppendMenuW(workerMenu, MF_STRING, IDM_BGE_LAUNCH_SAMPLE_TWO, L"Launch Sample Game Two");

    AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(workerMenu), L"&Workers");
    DrawMenuBar(hWnd);
}

void UpdateRoleWindowTitle(HWND hWnd)
{
    if (g_playerRuntimeMode) {
        SetWindowTextW(hWnd, g_playerRuntimeTitle.empty() ? L"Arcade Runtime" : g_playerRuntimeTitle.c_str());
        return;
    }

    std::wstring title = L"BasicGameEngine - ";
    if (g_isController) {
        title += L"Controller";
    }
    else {
        title += L"Worker: ";
        title += g_workerName;
    }
    SetWindowTextW(hWnd, title.c_str());
}

void BootstrapRoleOpNode()
{
    std::string role = g_isController ? "bge.controller" : Narrow(g_workerName);
    auto root = std::make_shared<OpNode>(role.empty() ? "bge.worker" : role);
    root->SetAttribute("bge.role", g_isController ? "controller" : "worker");
    root->SetAttribute("bge.worker.name", role);
    root->SetAttribute("runtime.game-loop", CurrentProcessOwnsGameLoop() ? "enabled" : "disabled");
    root->SetAttribute("plugin.opnode", "enabled");
    root->SetAttribute("plugin.game-loop", (role == "bge.game-loop") ? "enabled" : "available");
    root->SetAttribute("plugin.directx11", (role == "bge.game-loop") ? "enabled" : "available");
    root->SetAttribute("plugin.directx12", "available");
    root->SetAttribute("animation.2d.bouncing-ball", (role == "bge.game-loop") ? "enabled" : "available");
    root->SetAttribute("plugin.scene-3d", (role == "bge.scene-3d") ? "enabled" : "available");
    root->SetAttribute("plugin.images", (role == "bge.images") ? "enabled" : "available");
    root->SetAttribute("plugin.sound", (role == "bge.sound") ? "enabled" : "available");
    root->SetAttribute("plugin.sample-game", (role == "bge.sample-game-one" || role == "bge.sample-game-two") ? "enabled" : "available");
    AddBgePluginRegistryAttributesToOpNode(root);
    root->SetAttribute("environment", "basic-3d");
    if (role == "bge.game-loop") {
        root->AddOperation(std::make_shared<DirectX11BouncingBallOperation>());
        root->AddOperation(std::make_shared<BgeAudioPluginOperation>());
        root->AddOperation(std::make_shared<BgeBreakableRockFieldPluginOperation>());
        root->AddOperation(std::make_shared<BgeProjectilePluginOperation>());
        root->AddOperation(std::make_shared<BgeScoreboardPluginOperation>());
        root->AddOperation(std::make_shared<BgeTitleScreenPluginOperation>());
        root->AddOperation(std::make_shared<BgeVectorShipPluginOperation>());
    }
    root->AddOperation(std::make_shared<BasicGameRoleOperation>());
    root->PerformOperations();
}

bool DirectXRendererActive()
{
    if (g_rendererApi.load() == BgeRendererApi::DirectX12) {
        return g_directX12Renderer && g_directX12Renderer->IsInitialized();
    }
    return g_directX11Renderer && g_directX11Renderer->IsInitialized();
}

const char* RendererApiName()
{
    return g_rendererApi.load() == BgeRendererApi::DirectX12 ? "directx12" : "directx11";
}

std::wstring RendererClassName()
{
    return g_rendererApi.load() == BgeRendererApi::DirectX12 ? L"DirectX12BouncingBallRenderer" : L"DirectX11BouncingBallRenderer";
}

void LogRendererMessage(const std::string& message)
{
    std::ofstream log("BasicGameEngine.log", std::ios::app);
    log << message << "\n";
}

std::wstring CurrentTimeLabel()
{
    SYSTEMTIME localTime{};
    GetLocalTime(&localTime);
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%02u:%02u:%02u", localTime.wHour, localTime.wMinute, localTime.wSecond);
    return buffer;
}

void UpdateControllerHistoryDetail(const std::wstring& detailText)
{
    if (g_controllerHistoryDetail) {
        SetWindowTextW(g_controllerHistoryDetail, detailText.empty() ? L"No command structure recorded for this event." : detailText.c_str());
    }
}

void RefreshControllerHistoryWindow()
{
    if (!g_controllerHistoryList) {
        return;
    }

    SendMessageW(g_controllerHistoryList, LB_RESETCONTENT, 0, 0);
    for (const auto& item : g_controllerHistory) {
        SendMessageW(g_controllerHistoryList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
    }
}

void AddControllerHistory(const std::wstring& historyText, const std::wstring& detailText)
{
    if (!g_isController) {
        return;
    }

    std::wstring entry = CurrentTimeLabel() + L"  " + historyText;
    g_controllerHistory.push_back(entry);
    g_controllerHistoryDetails.push_back(detailText.empty() ? L"No command structure recorded for this event." : detailText);
    if (g_controllerHistory.size() > BGE_CONTROLLER_HISTORY_LIMIT) {
        g_controllerHistory.erase(g_controllerHistory.begin());
        if (!g_controllerHistoryDetails.empty()) {
            g_controllerHistoryDetails.erase(g_controllerHistoryDetails.begin());
        }
    }

    if (g_controllerHistoryList) {
        RefreshControllerHistoryWindow();
        if (!g_controllerHistory.empty()) {
            SendMessageW(g_controllerHistoryList, LB_SETCURSEL, static_cast<WPARAM>(g_controllerHistory.size() - 1), 0);
        }
    }

    UpdateControllerHistoryDetail(g_controllerHistoryDetails.empty() ? L"" : g_controllerHistoryDetails.back());
}

void UpdateControllerHistoryDetailFromSelection()
{
    if (!g_controllerHistoryList) {
        return;
    }

    LRESULT selection = SendMessageW(g_controllerHistoryList, LB_GETCURSEL, 0, 0);
    if (selection == LB_ERR) {
        return;
    }

    size_t detailIndex = static_cast<size_t>(selection);
    if (detailIndex < g_controllerHistoryDetails.size()) {
        UpdateControllerHistoryDetail(g_controllerHistoryDetails[detailIndex]);
    }
}

std::wstring TrimText(const std::wstring& value)
{
    size_t first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return L"";
    }
    size_t last = value.find_last_not_of(L" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::wstring ConstructionArtifactExtension(const std::wstring& path)
{
    size_t slash = path.find_last_of(L"\\/");
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) {
        return L"";
    }
    return LowerArg(path.substr(dot));
}

bool ExtractLineConstructionCommands(const std::wstring& text, std::vector<std::wstring>& commands)
{
    std::wstringstream stream(text);
    std::wstring line;
    while (std::getline(stream, line)) {
        std::wstring trimmed = TrimText(line);
        if (trimmed.empty() || trimmed.rfind(L"#", 0) == 0 || trimmed.rfind(L"//", 0) == 0) {
            continue;
        }
        commands.push_back(trimmed);
    }
    return !commands.empty();
}

bool ConsumeJsonString(const std::wstring& text, size_t& index, std::wstring& value)
{
    if (index >= text.size() || text[index] != L'\"') {
        return false;
    }
    ++index;
    value.clear();
    while (index < text.size()) {
        wchar_t ch = text[index++];
        if (ch == L'\"') {
            return true;
        }
        if (ch != L'\\') {
            value += ch;
            continue;
        }
        if (index >= text.size()) {
            return false;
        }
        wchar_t escaped = text[index++];
        switch (escaped) {
        case L'\"': value += L'\"'; break;
        case L'\\': value += L'\\'; break;
        case L'/': value += L'/'; break;
        case L'b': value += L'\b'; break;
        case L'f': value += L'\f'; break;
        case L'n': value += L'\n'; break;
        case L'r': value += L'\r'; break;
        case L't': value += L'\t'; break;
        case L'u':
            if (index + 4 <= text.size()) {
                index += 4;
                value += L'?';
            }
            else {
                return false;
            }
            break;
        default:
            value += escaped;
            break;
        }
    }
    return false;
}

bool ExtractJsonConstructionCommands(const std::wstring& text, std::vector<std::wstring>& commands, std::wstring& errorText)
{
    size_t key = text.find(L"\"commands\"");
    if (key == std::wstring::npos) {
        errorText = L"JSON construction artifact needs a commands array";
        return false;
    }

    size_t colon = text.find(L':', key + 10);
    size_t arrayStart = colon == std::wstring::npos ? std::wstring::npos : text.find(L'[', colon + 1);
    if (arrayStart == std::wstring::npos) {
        errorText = L"JSON commands must be an array";
        return false;
    }

    size_t index = arrayStart + 1;
    while (index < text.size()) {
        while (index < text.size() && iswspace(text[index])) {
            ++index;
        }
        if (index < text.size() && text[index] == L']') {
            break;
        }
        if (index < text.size() && text[index] == L',') {
            ++index;
            continue;
        }
        std::wstring command;
        if (!ConsumeJsonString(text, index, command)) {
            errorText = L"JSON commands array must contain command strings";
            return false;
        }
        command = TrimText(command);
        if (!command.empty()) {
            commands.push_back(command);
        }
    }

    if (commands.empty()) {
        errorText = L"JSON commands array is empty";
        return false;
    }
    return true;
}

std::vector<std::wstring> ParseCsvConstructionRecord(const std::wstring& line)
{
    std::vector<std::wstring> fields;
    std::wstring field;
    bool inQuotes = false;

    for (size_t index = 0; index < line.size(); ++index) {
        wchar_t ch = line[index];
        if (inQuotes && ch == L'\"' && index + 1 < line.size() && line[index + 1] == L'\"') {
            field += L'\"';
            ++index;
            continue;
        }
        if (ch == L'\"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (!inQuotes && ch == L',') {
            fields.push_back(TrimText(field));
            field.clear();
            continue;
        }
        field += ch;
    }
    fields.push_back(TrimText(field));
    return fields;
}

bool ExtractCsvConstructionCommands(const std::wstring& text, std::vector<std::wstring>& commands, std::wstring& errorText)
{
    std::wstringstream stream(text);
    std::wstring line;
    int commandColumn = -1;
    bool headerRead = false;

    while (std::getline(stream, line)) {
        std::wstring trimmed = TrimText(line);
        if (trimmed.empty() || trimmed.rfind(L"#", 0) == 0 || trimmed.rfind(L"//", 0) == 0) {
            continue;
        }

        std::vector<std::wstring> fields = ParseCsvConstructionRecord(trimmed);
        if (!headerRead) {
            headerRead = true;
            bool commandRow = fields.size() >= 2 && LowerArg(fields[0]) == L"command" && !TrimText(fields[1]).empty();
            if (!commandRow) {
                for (size_t index = 0; index < fields.size(); ++index) {
                    if (LowerArg(fields[index]) == L"command") {
                        commandColumn = static_cast<int>(index);
                        break;
                    }
                }
                if (commandColumn >= 0) {
                    continue;
                }
            }
        }

        std::wstring command;
        if (commandColumn >= 0 && static_cast<size_t>(commandColumn) < fields.size()) {
            command = fields[static_cast<size_t>(commandColumn)];
        }
        else if (fields.size() >= 2 && LowerArg(fields[0]) == L"command") {
            command = fields[1];
        }
        else if (fields.size() == 1) {
            command = fields[0];
        }
        command = TrimText(command);
        if (!command.empty()) {
            commands.push_back(command);
        }
    }

    if (commands.empty()) {
        errorText = L"CSV construction artifact needs a command column or command rows";
        return false;
    }
    return true;
}

bool LoadConstructionArtifactCommands(const std::wstring& path, std::vector<std::wstring>& commands, std::wstring& errorText)
{
    std::ifstream input(Narrow(path), std::ios::binary);
    if (!input) {
        errorText = L"Could not open construction artifact: " + path;
        return false;
    }

    std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::wstring text = WidenUtf8(bytes);
    if (text.empty() && !bytes.empty()) {
        errorText = L"Could not read construction artifact text";
        return false;
    }

    std::wstring extension = ConstructionArtifactExtension(path);
    if (extension == L".json") {
        return ExtractJsonConstructionCommands(text, commands, errorText);
    }
    if (extension == L".csv") {
        return ExtractCsvConstructionCommands(text, commands, errorText);
    }
    if (!ExtractLineConstructionCommands(text, commands)) {
        errorText = L"Construction artifact has no commands";
        return false;
    }
    return true;
}

std::wstring SelectorKey(const std::wstring& value)
{
    std::wstring lower = LowerArg(value);
    std::wstring key;
    for (wchar_t character : lower) {
        if ((character >= L'a' && character <= L'z') || (character >= L'0' && character <= L'9')) {
            key += character;
        }
    }
    return key;
}

std::wstring RoleShortName(const std::wstring& role)
{
    if (role.rfind(L"bge.", 0) == 0) {
        return role.substr(4);
    }
    return role;
}

int ArtifactIndexForRole(const std::wstring& role)
{
    std::wstring roleLower = LowerArg(role);
    for (int artifactIndex = 0; artifactIndex < BGE_CONTROLLER_ARTIFACT_COUNT; ++artifactIndex) {
        if (roleLower == LowerArg(kControllerArtifacts[artifactIndex].role)) {
            return artifactIndex;
        }
    }
    return -1;
}

int ResolveControllerArtifactIndex(const std::wstring& selector)
{
    if (selector.empty()) {
        return g_selectedControllerArtifact;
    }

    std::wstring key = SelectorKey(selector);
    if (key.empty()) {
        return g_selectedControllerArtifact;
    }

    if (key == L"object" || key == L"objects" || key == L"vector" || key == L"vectors" || key == L"render" || key == L"renderer") {
        return ArtifactIndexForRole(L"bge.game-loop");
    }
    if (key == L"asset" || key == L"assets" || key == L"image" || key == L"images" || key == L"background" || key == L"backgrounds") {
        return ArtifactIndexForRole(L"bge.images");
    }
    if (key == L"audio" || key == L"sound") {
        return ArtifactIndexForRole(L"bge.sound");
    }

    for (int artifactIndex = 0; artifactIndex < BGE_CONTROLLER_ARTIFACT_COUNT; ++artifactIndex) {
        const BgeControllerArtifactSpec& spec = kControllerArtifacts[artifactIndex];
        if (key == SelectorKey(spec.role) || key == SelectorKey(RoleShortName(spec.role)) || key == SelectorKey(spec.displayName) || key == SelectorKey(spec.group)) {
            return artifactIndex;
        }
    }
    return -1;
}

std::wstring SelectedControllerRole()
{
    if (g_selectedControllerArtifact < 0 || g_selectedControllerArtifact >= BGE_CONTROLLER_ARTIFACT_COUNT) {
        return L"";
    }
    return kControllerArtifacts[g_selectedControllerArtifact].role;
}

bool RoleVectorContains(const std::vector<std::wstring>& roles, const std::wstring& role)
{
    std::wstring roleLower = LowerArg(role);
    for (const auto& existingRole : roles) {
        if (LowerArg(existingRole) == roleLower) {
            return true;
        }
    }
    return false;
}

void AddRevealedWorkerRole(const std::wstring& role)
{
    if (!RoleVectorContains(g_revealedWorkerRoles, role)) {
        g_revealedWorkerRoles.push_back(role);
    }
}

void RemoveRevealedWorkerRole(const std::wstring& role)
{
    std::wstring roleLower = LowerArg(role);
    g_revealedWorkerRoles.erase(std::remove_if(g_revealedWorkerRoles.begin(), g_revealedWorkerRoles.end(), [&roleLower](const std::wstring& existingRole) {
        return LowerArg(existingRole) == roleLower;
    }), g_revealedWorkerRoles.end());
}

struct WorkerSnapshot {
    bool running = false;
    DWORD pid = 0;
    DWORD64 heartbeatAgeMs = 0;
    HWND window = nullptr;
    bool windowVisible = false;
};

struct FindWindowContext {
    DWORD pid;
    HWND window;
};

BOOL CALLBACK FindWindowForProcessCallback(HWND hwnd, LPARAM lParam)
{
    FindWindowContext* context = reinterpret_cast<FindWindowContext*>(lParam);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid == context->pid && GetWindow(hwnd, GW_OWNER) == nullptr && IsWindow(hwnd)) {
        wchar_t title[256]{};
        GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
        if (wcsstr(title, L"BasicGameEngine - Worker:") || wcsstr(title, L"BasicGameEngine - Controller")) {
            context->window = hwnd;
            return FALSE;
        }
        if (!context->window) {
            context->window = hwnd;
        }
    }
    return TRUE;
}

HWND FindWindowForProcess(DWORD pid)
{
    FindWindowContext context{ pid, nullptr };
    EnumWindows(FindWindowForProcessCallback, reinterpret_cast<LPARAM>(&context));
    return context.window;
}

WorkerSnapshot GetWorkerSnapshot(const std::wstring& role)
{
    WorkerSnapshot snapshot;
    if (!g_sharedData || !EnsureCoordMutex()) {
        return snapshot;
    }

    DWORD64 now = GetTickCount64();
    {
        CoordLock lock(g_coordMutex);
        if (!lock.locked()) {
            return snapshot;
        }

        PruneStaleWorkersLocked(g_sharedData);
        for (LONG workerIndex = 0; workerIndex < BGE_MAX_WORKERS; ++workerIndex) {
            const SharedWorkerData& worker = g_sharedData->workers[workerIndex];
            if (worker.pid != 0 && role == worker.name) {
                snapshot.running = true;
                snapshot.pid = worker.pid;
                snapshot.heartbeatAgeMs = now >= worker.heartbeatTick ? now - worker.heartbeatTick : 0;
                break;
            }
        }
    }

    if (snapshot.running) {
        snapshot.window = FindWindowForProcess(snapshot.pid);
        snapshot.windowVisible = snapshot.window && IsWindowVisible(snapshot.window);
    }
    return snapshot;
}

std::wstring WindowVisibilityText(const WorkerSnapshot& snapshot)
{
    if (!snapshot.running) {
        return L"not running";
    }
    if (!snapshot.window) {
        return L"window missing";
    }
    return snapshot.windowVisible ? L"visible" : L"hidden";
}

std::wstring ConstructionArtifactDetail(const std::wstring& path, const std::vector<std::wstring>& commands)
{
    std::wstring detail = L"Construction artifact:\r\n  " + path + L"\r\n\r\nCLI equivalent:\r\n  " + ControllerBaseCli() + L" --launch-basic-game --game-file " + QuoteArg(path) + L"\r\n\r\nQueued commands:";
    for (const std::wstring& command : commands) {
        detail += L"\r\n  " + command;
    }
    return detail;
}

bool QueueConstructionArtifactCommandsFromFile(const std::wstring& path, std::wstring& statusText)
{
    if (!g_isController) {
        statusText = L"Construction artifacts load from the BGE controller";
        return false;
    }

    std::vector<std::wstring> commands;
    std::wstring errorText;
    if (!LoadConstructionArtifactCommands(path, commands, errorText)) {
        statusText = errorText;
        return false;
    }

    int gameLoopIndex = ArtifactIndexForRole(L"bge.game-loop");
    if (gameLoopIndex >= 0) {
        SelectControllerArtifact(gameLoopIndex);
    }
    WorkerSnapshot snapshot = GetWorkerSnapshot(L"bge.game-loop");
    if (!snapshot.running) {
        LaunchWorkerRole(L"bge.game-loop");
    }

    for (const std::wstring& command : commands) {
        g_pendingControllerCommands.push_back(command);
    }

    statusText = L"Queued " + std::to_wstring(commands.size()) + L" construction commands";
    AddControllerHistory(L"Load construction artifact -> " + path, ConstructionArtifactDetail(path, commands));
    SyncControllerControls();
    return true;
}

std::wstring StripLocalWorkerTargetPrefix(const std::wstring& commandText)
{
    size_t colon = commandText.find(L':');
    if (colon == std::wstring::npos) {
        return commandText;
    }

    std::wstring selector = LowerArg(TrimText(commandText.substr(0, colon)));
    if (selector == L"game-loop" || selector == L"bge.game-loop" || selector == L"worker" || selector == L"scene and render") {
        return TrimText(commandText.substr(colon + 1));
    }
    return commandText;
}

bool ExecuteConstructionCommandsLocally(const std::vector<std::wstring>& commands, const std::wstring& sourceLabel, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"Construction artifacts need the game-loop runtime";
        return false;
    }

    size_t executedCount = 0;
    size_t skippedCount = 0;
    size_t failedCount = 0;
    for (const std::wstring& command : commands) {
        std::wstring localCommand = StripLocalWorkerTargetPrefix(command);
        std::vector<std::wstring> tokens = TokenizeCommandText(localCommand);
        if (tokens.empty()) {
            ++skippedCount;
            continue;
        }
        if (LowerArg(tokens[0]) == L"export") {
            ++skippedCount;
            continue;
        }

        std::wstring commandStatus;
        if (ExecuteCommandText(localCommand, commandStatus)) {
            ++executedCount;
            SendWorkerTelemetry(L"script-command", localCommand, commandStatus);
        }
        else {
            ++failedCount;
            SendWorkerTelemetry(L"script-command-failed", localCommand, commandStatus);
            LogRendererMessage("[BgePlayerRuntime] command-failed command=\"" + Narrow(localCommand) + "\" status=\"" + Narrow(commandStatus) + "\"");
        }
    }

    statusText = L"Player runtime loaded " + std::to_wstring(executedCount) + L" commands from " + sourceLabel;
    if (skippedCount > 0) {
        statusText += L", skipped " + std::to_wstring(skippedCount);
    }
    if (failedCount > 0) {
        statusText += L", failed " + std::to_wstring(failedCount);
    }
    return executedCount > 0 || failedCount == 0;
}

bool ExecuteConstructionArtifactCommandsLocally(const std::wstring& path, std::wstring& statusText)
{
    std::vector<std::wstring> commands;
    std::wstring errorText;
    if (!LoadConstructionArtifactCommands(path, commands, errorText)) {
        statusText = errorText;
        return false;
    }
    return ExecuteConstructionCommandsLocally(commands, path, statusText);
}

bool LoadEmbeddedGameScriptCommands(std::vector<std::wstring>& commands, std::wstring& errorText)
{
    HRSRC resource = FindResourceW(nullptr, kBgeEmbeddedGameScriptResourceName, RT_RCDATA);
    if (!resource) {
        errorText = L"No embedded BGE game script";
        return false;
    }

    DWORD size = SizeofResource(nullptr, resource);
    HGLOBAL handle = LoadResource(nullptr, resource);
    const char* bytes = handle ? static_cast<const char*>(LockResource(handle)) : nullptr;
    if (!bytes || size == 0) {
        errorText = L"Embedded BGE game script is empty";
        return false;
    }

    std::wstring scriptText = WidenUtf8(std::string(bytes, bytes + size));
    if (!ExtractLineConstructionCommands(scriptText, commands)) {
        errorText = L"Embedded BGE game script has no commands";
        return false;
    }
    return true;
}

void ProcessPlayerRuntimeAutomation()
{
    if (!g_playerRuntimeMode || g_playerRuntimeCommandsProcessed || !CurrentProcessOwnsGameLoop()) {
        return;
    }

    g_playerRuntimeCommandsProcessed = true;
    if (g_cliConstructionArtifactPaths.empty()) {
        std::vector<std::wstring> embeddedCommands;
        std::wstring statusText;
        if (LoadEmbeddedGameScriptCommands(embeddedCommands, statusText)) {
            ExecuteConstructionCommandsLocally(embeddedCommands, L"embedded resource", statusText);
        }
        SetCommandStatus(statusText);
        LogRendererMessage("[BgePlayerRuntime] embedded-script status=\"" + Narrow(statusText) + "\"");
        return;
    }

    for (const std::wstring& artifactPath : g_cliConstructionArtifactPaths) {
        std::wstring statusText;
        ExecuteConstructionArtifactCommandsLocally(artifactPath, statusText);
        SetCommandStatus(statusText);
        LogRendererMessage("[BgePlayerRuntime] script=\"" + Narrow(artifactPath) + "\" status=\"" + Narrow(statusText) + "\"");
    }
}

std::wstring SafeExportFileStem(std::wstring value)
{
    value = TrimText(value);
    if (value.empty()) {
        return L"space-rocks";
    }

    for (wchar_t& ch : value) {
        if (ch == L' ' || ch == L'\t') {
            ch = L'-';
        }
        else if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*') {
            ch = L'-';
        }
    }
    return value;
}

bool WriteUtf8TextFile(const std::filesystem::path& path, const std::wstring& text, std::wstring& errorText)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        errorText = L"Could not write " + path.wstring();
        return false;
    }
    std::string bytes = Narrow(text);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        errorText = L"Could not finish writing " + path.wstring();
        return false;
    }
    return true;
}

bool EmbedUtf8TextResourceInExecutable(const std::filesystem::path& executablePath, const wchar_t* resourceName, const std::wstring& text, std::wstring& errorText)
{
    HANDLE update = BeginUpdateResourceW(executablePath.c_str(), FALSE);
    if (!update) {
        errorText = L"Could not open executable resources: " + executablePath.wstring();
        return false;
    }

    std::string bytes = Narrow(text);
    BOOL updated = UpdateResourceW(update, RT_RCDATA, resourceName, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), bytes.empty() ? nullptr : bytes.data(), static_cast<DWORD>(bytes.size()));
    if (!updated) {
        EndUpdateResourceW(update, TRUE);
        errorText = L"Could not embed BGE resource in " + executablePath.wstring();
        return false;
    }

    if (!EndUpdateResourceW(update, FALSE)) {
        errorText = L"Could not finish executable resource update: " + executablePath.wstring();
        return false;
    }
    return true;
}

void AddExportFeatureSet(std::vector<std::wstring>& features, const std::wstring& featureId)
{
    if (featureId.empty()) {
        return;
    }
    std::wstring lowerFeature = LowerArg(featureId);
    if (std::any_of(features.begin(), features.end(), [&lowerFeature](const std::wstring& existing) { return LowerArg(existing) == lowerFeature; })) {
        return;
    }
    features.push_back(featureId);
}

std::vector<std::wstring> BuildExportFeatureSetLedger(const std::vector<std::wstring>& commands)
{
    std::vector<std::wstring> features;
    for (const std::wstring& command : commands) {
        std::vector<std::wstring> tokens = TokenizeCommandText(StripLocalWorkerTargetPrefix(command));
        if (tokens.empty()) {
            continue;
        }

        std::wstring verb = LowerArg(tokens[0]);
        if ((verb == L"plugin" || verb == L"capability") && tokens.size() >= 3 && LowerArg(tokens[1]) == L"import") {
            std::wstring pluginId = LowerArg(tokens[2]);
            if (pluginId.rfind(L"bge.piece.", 0) == 0) {
                AddExportFeatureSet(features, pluginId);
            }
        }
        else if (verb == L"player-ship" || verb == L"vector-ship") {
            AddExportFeatureSet(features, L"bge.piece.vector-ship");
        }
        else if (verb == L"projectile") {
            AddExportFeatureSet(features, L"bge.piece.projectile");
        }
        else if (verb == L"rock-field" || verb == L"breakable-rock-field") {
            AddExportFeatureSet(features, L"bge.piece.breakable-rock-field");
        }
        else if (verb == L"ufo") {
            AddExportFeatureSet(features, L"bge.piece.ufo");
        }
        else if (verb == L"scoreboard") {
            AddExportFeatureSet(features, L"bge.piece.scoreboard");
        }
        else if (verb == L"title-screen") {
            AddExportFeatureSet(features, L"bge.piece.title-screen");
        }
    }
    return features;
}

std::wstring BuildExportFeatureVersion(const std::vector<std::wstring>& features)
{
    return L"0." + std::to_wstring(features.size()) + L".0";
}

std::wstring BuildExportFeatureLedgerText(const std::wstring& exportName, const std::vector<std::wstring>& features)
{
    std::wstring text = L"name=" + exportName + L"\r\n";
    text += L"version=" + BuildExportFeatureVersion(features) + L"\r\n";
    text += L"version-policy=major runtime/save break, minor feature set, patch fix\r\n";
    text += L"feature-count=" + std::to_wstring(features.size()) + L"\r\n";
    for (size_t index = 0; index < features.size(); ++index) {
        text += L"feature." + std::to_wstring(index + 1) + L"=" + features[index] + L"\r\n";
    }
    return text;
}

std::vector<std::wstring> BuildExportCommandsFromCurrentState()
{
    std::vector<std::wstring> commands;

    for (const std::wstring& pluginId : g_importedBgePlugins) {
        commands.push_back(L"plugin import " + CommandFileArg(pluginId));
    }

    BgeTitleScreenState titleScreen;
    BgeScoreboardState scoreboard;
    BgeBreakableRockFieldState rockField;
    BgeUfoState ufo;
    std::vector<BgeCounterState> counters;
    std::vector<BgeProjectileDefinition> projectiles;
    BgeVectorShipState vectorShip;
    bool titleScreenActive = false;
    bool scoreboardActive = false;
    bool rockFieldActive = false;
    bool ufoActive = false;
    bool vectorShipActive = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        titleScreen = g_titleScreenState;
        scoreboard = g_scoreboardState;
        rockField = g_breakableRockFieldState;
        ufo = g_ufoState;
        counters = g_bgeCounters;
        projectiles = g_projectileDefinitions;
        vectorShip = g_vectorShipState;
        titleScreenActive = g_titleScreenActive && g_titleScreenState.visible;
        scoreboardActive = g_scoreboardActive && g_scoreboardState.visible;
        rockFieldActive = g_breakableRockFieldActive && g_breakableRockFieldState.active;
        ufoActive = g_ufoActive && g_ufoState.active;
        vectorShipActive = g_vectorShipActive && g_vectorShipState.active;
    }

    for (const BgeCounterState& counter : counters) {
        std::wstring command = L"counter define " + CommandFileArg(counter.name) + L" " + std::to_wstring(counter.value);
        if (counter.hasMin) {
            command += L" min " + std::to_wstring(counter.minValue);
        }
        if (counter.hasMax) {
            command += L" max " + std::to_wstring(counter.maxValue);
        }
        if (counter.persistSession) {
            command += L" persist session";
        }
        commands.push_back(command);
    }

    for (const BgeProjectileDefinition& projectile : projectiles) {
        std::wstring command = L"projectile create --id " + CommandFileArg(projectile.id)
            + L" --owner " + CommandFileArg(projectile.owner)
            + L" --shape " + CommandFileArg(BgeObjectShapeName(projectile.shape))
            + L" --speed " + std::to_wstring(static_cast<int>(projectile.speed))
            + L" --ttl " + std::to_wstring(projectile.ttlSeconds)
            + L" --collision-tag " + CommandFileArg(projectile.collisionTag);
        command += projectile.wrap ? L" --wrap" : L" --no-wrap";
        commands.push_back(command);
    }

    if (ufoActive) {
        std::wstring command = L"ufo create --id " + CommandFileArg(ufo.id)
            + L" --score " + CommandFileArg(ufo.score)
            + L" --weapon " + CommandFileArg(ufo.weapon)
            + L" --arrival " + std::to_wstring(static_cast<int>(ufo.arrivalMinSeconds)) + L".." + std::to_wstring(static_cast<int>(ufo.arrivalMaxSeconds))
            + L" --aim " + CommandFileArg(ufo.aim)
            + L" --radius " + std::to_wstring(static_cast<int>(ufo.radius))
            + L" --speed " + std::to_wstring(static_cast<int>(ufo.speed))
            + L" --style " + CommandFileArg(BgeObjectRenderStyleName(ufo.renderStyle))
            + L" --outline-thickness " + std::to_wstring(ufo.outlineThickness);
        command += ufo.edgeSpawn ? L" --edge-spawn" : L" --no-edge-spawn";
        commands.push_back(command);
    }

    if (vectorShipActive) {
        commands.push_back(L"player-ship create --id " + CommandFileArg(vectorShip.id)
            + L" --shape " + CommandFileArg(BgeObjectShapeName(vectorShip.shape))
            + L" --lives " + CommandFileArg(vectorShip.lives)
            + L" --input-profile " + CommandFileArg(vectorShip.inputProfile)
            + L" --fire " + CommandFileArg(vectorShip.fire)
            + L" --hyperspace " + CommandFileArg(vectorShip.hyperspace)
            + L" --respawn " + std::to_wstring(vectorShip.respawnSeconds)
            + L" --invulnerable " + std::to_wstring(vectorShip.invulnerableSeconds));
    }

    if (rockFieldActive) {
        commands.push_back(L"rock-field create --sizes " + CommandFileArg(JoinBgeListText(rockField.sizes))
            + L" --count " + std::to_wstring(rockField.count)
            + L" --split " + std::to_wstring(rockField.splitCount)
            + L" --score " + CommandFileArg(rockField.score)
            + L" --speed-range " + std::to_wstring(static_cast<int>(rockField.speedMin)) + L".." + std::to_wstring(static_cast<int>(rockField.speedMax))
            + L" --wave-counter " + CommandFileArg(rockField.waveCounter)
            + L" --avoid " + CommandFileArg(rockField.avoid + L":" + std::to_wstring(static_cast<int>(rockField.avoidRadius))));
    }

    if (scoreboardActive) {
        std::wstring countersValue;
        for (size_t index = 0; index < scoreboard.counters.size(); ++index) {
            if (index > 0) {
                countersValue += L"|";
            }
            countersValue += scoreboard.counters[index];
        }
        commands.push_back(L"scoreboard create --counters " + CommandFileArg(countersValue)
            + L" --anchor " + CommandFileArg(scoreboard.anchor)
            + L" --format " + CommandFileArg(scoreboard.format)
            + L" --font " + CommandFileArg(scoreboard.font)
            + L" --color " + CommandFileArg(scoreboard.color)
            + L" --scale " + std::to_wstring(scoreboard.scale));
    }

    if (titleScreenActive) {
        std::wstring command = L"title-screen create --text " + CommandFileArg(titleScreen.text)
            + L" --subtitle " + CommandFileArg(titleScreen.subtitle)
            + L" --start " + CommandFileArg(titleScreen.start)
            + L" --next " + CommandFileArg(titleScreen.next)
            + L" --font " + CommandFileArg(titleScreen.font);
        if (titleScreen.showLegend) {
            command += L" --legend " + CommandFileArg(titleScreen.legend);
        }
        else {
            command += L" --no-legend";
        }
        if (titleScreen.showCredit) {
            command += L" --credit " + CommandFileArg(titleScreen.credit);
        }
        else {
            command += L" --no-credit";
        }
        command += titleScreen.centered ? L" --center" : L" --left";
        command += titleScreen.blinkPrompt ? L" --blink" : L" --no-blink";
        commands.push_back(command);
    }

    return commands;
}

std::vector<std::wstring> BuildExportCommandScript(const std::wstring& fromMode)
{
    if ((fromMode.empty() || fromMode == L"history") && !g_workerCommandHistory.empty()) {
        return g_workerCommandHistory;
    }
    return BuildExportCommandsFromCurrentState();
}

bool ExecuteBgeExportCommand(const std::vector<std::wstring>& tokens, std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"export commands run in bge.game-loop";
        return false;
    }

    std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
    if (subcommand == L"enable") {
        statusText = L"Executable export enabled for game-loop history";
        return true;
    }
    if (subcommand != L"executable") {
        statusText = L"Use: export executable --target windows-x64 --name space-rocks --from history";
        return false;
    }

    std::wstring target = L"windows-x64";
    std::wstring exportName = L"space-rocks";
    std::wstring fromMode = L"history";
    std::wstring value;
    if (TryGetCommandOptionValue(tokens, L"--target", value)) {
        target = LowerArg(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--name", value)) {
        exportName = SafeExportFileStem(value);
    }
    if (TryGetCommandOptionValue(tokens, L"--from", value)) {
        fromMode = LowerArg(value);
    }
    if (target != L"windows-x64") {
        statusText = L"Only windows-x64 executable export is available";
        return false;
    }

    std::vector<std::wstring> commands = BuildExportCommandScript(fromMode);
    if (commands.empty()) {
        statusText = L"No game commands available to export";
        return false;
    }

    std::filesystem::path packageDir = std::filesystem::current_path() / "exports" / Narrow(exportName);
    std::error_code fsError;
    std::filesystem::create_directories(packageDir, fsError);
    if (fsError) {
        statusText = L"Could not create export directory: " + packageDir.wstring();
        return false;
    }

    std::filesystem::path sourceExe = ExePath();
    std::filesystem::path exportedExe = packageDir / (Narrow(exportName) + ".exe");
    fsError.clear();
    if (!std::filesystem::equivalent(sourceExe, exportedExe, fsError)) {
        fsError.clear();
        std::filesystem::copy_file(sourceExe, exportedExe, std::filesystem::copy_options::overwrite_existing, fsError);
        if (fsError) {
            statusText = L"Could not copy executable to " + exportedExe.wstring();
            return false;
        }
    }

    std::filesystem::path scriptPath = packageDir / (Narrow(exportName) + ".commands");
    std::wstring scriptText = L"# Generated by BasicGameEngine export executable\r\n";
    for (const std::wstring& command : commands) {
        scriptText += command + L"\r\n";
    }

    std::vector<std::wstring> featureSets = BuildExportFeatureSetLedger(commands);
    std::wstring featureVersion = BuildExportFeatureVersion(featureSets);
    std::wstring featureLedgerText = BuildExportFeatureLedgerText(exportName, featureSets);

    std::wstring errorText;
    if (!WriteUtf8TextFile(scriptPath, scriptText, errorText)) {
        statusText = errorText;
        return false;
    }

    std::filesystem::path featureLedgerPath = packageDir / (Narrow(exportName) + ".features.txt");
    if (!WriteUtf8TextFile(featureLedgerPath, featureLedgerText, errorText)) {
        statusText = errorText;
        return false;
    }

    if (!EmbedUtf8TextResourceInExecutable(exportedExe, kBgeEmbeddedGameScriptResourceName, scriptText, errorText)) {
        statusText = errorText;
        return false;
    }
    if (!EmbedUtf8TextResourceInExecutable(exportedExe, kBgeEmbeddedFeatureLedgerResourceName, featureLedgerText, errorText)) {
        statusText = errorText;
        return false;
    }

    std::filesystem::path launcherPath = packageDir / (std::wstring(L"launch-") + exportName + L".cmd");
    std::wstring launcherText = L"@echo off\r\nstart \"\" \"%~dp0" + exportedExe.filename().wstring() + L"\"\r\n";
    if (!WriteUtf8TextFile(launcherPath, launcherText, errorText)) {
        statusText = errorText;
        return false;
    }

    std::filesystem::path manifestPath = packageDir / (Narrow(exportName) + ".export.txt");
    std::wstring manifestText = L"name=" + exportName + L"\r\n"
        + L"version=" + featureVersion + L"\r\n"
        + L"feature-count=" + std::to_wstring(featureSets.size()) + L"\r\n"
        + L"target=" + target + L"\r\n"
        + L"runtime=player\r\n"
        + L"executable=" + exportedExe.filename().wstring() + L"\r\n"
        + L"commands=" + scriptPath.filename().wstring() + L"\r\n"
        + L"features=" + featureLedgerPath.filename().wstring() + L"\r\n"
        + L"embedded-commands=" + std::wstring(kBgeEmbeddedGameScriptResourceName) + L"\r\n"
        + L"embedded-features=" + std::wstring(kBgeEmbeddedFeatureLedgerResourceName) + L"\r\n"
        + L"launch=" + launcherPath.filename().wstring() + L"\r\n";
    if (!WriteUtf8TextFile(manifestPath, manifestText, errorText)) {
        statusText = errorText;
        return false;
    }

    statusText = L"Exported " + exportName + L" " + featureVersion + L" executable package: " + exportedExe.wstring();
    LogRendererMessage("[BgeExport] executable package=\"" + Narrow(packageDir.wstring()) + "\" name=\"" + Narrow(exportName) + "\" version=\"" + Narrow(featureVersion) + "\" commands=" + std::to_string(commands.size()));
    return true;
}

void PlaceWorkerWindow(HWND workerWindow, const std::wstring& role)
{
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int availableWidth = (std::max)(640, static_cast<int>(workArea.right - workArea.left));
    int availableHeight = (std::max)(480, static_cast<int>(workArea.bottom - workArea.top));
    int width = (std::min)(availableWidth - 64, (std::max)(880, availableWidth - 160));
    int height = (std::min)(availableHeight - 96, (std::max)(620, availableHeight - 144));
    int x = workArea.right - width - 32;
    int y = workArea.top + 48;

    if (role == L"bge.sound" || role == L"bge.images") {
        width = (std::min)(availableWidth - 96, 760);
        height = (std::min)(availableHeight - 128, 480);
        x = workArea.right - width - 56;
        y = workArea.top + 96;
    }
    else if (role == L"bge.scene-3d") {
        x = workArea.right - width - 72;
        y = workArea.top + 72;
    }

    x = (std::max)(static_cast<int>(workArea.left) + 16, x);
    y = (std::max)(static_cast<int>(workArea.top) + 16, y);
    width = (std::max)(BGE_MIN_WORKER_CLIENT_WIDTH, width);
    height = (std::max)(320, height);
    SetWindowPos(workerWindow, HWND_TOP, x, y, width, height, SWP_SHOWWINDOW);
}

bool InspectWorkerWindowByRole(const std::wstring& role, std::wstring& statusText)
{
    WorkerSnapshot snapshot = GetWorkerSnapshot(role);
    int artifactIndex = ArtifactIndexForRole(role);
    std::wstring displayName = artifactIndex >= 0 ? kControllerArtifacts[artifactIndex].displayName : role;

    if (!snapshot.running) {
        statusText = displayName + L" is not running";
        AddControllerHistory(L"Open failed -> " + displayName + L" is not running", InspectCommandDetail(role));
        return false;
    }
    if (!snapshot.window) {
        statusText = displayName + L" window not found";
        AddControllerHistory(L"Open failed -> " + displayName + L" window not found", InspectCommandDetail(role));
        return false;
    }

    ShowWindow(snapshot.window, SW_SHOWNORMAL);
    PlaceWorkerWindow(snapshot.window, role);
    SetForegroundWindow(snapshot.window);
    AddRevealedWorkerRole(role);
    statusText = L"Inspecting " + displayName;
    AddControllerHistory(L"Open window -> " + displayName + L" (" + role + L")", InspectCommandDetail(role));

    std::ostringstream message;
    message << "[BgeControllerUi] inspect role=" << Narrow(role) << " pid=" << snapshot.pid << " hwnd=" << reinterpret_cast<uintptr_t>(snapshot.window) << " result=shown";
    LogRendererMessage(message.str());
    return true;
}

bool HideWorkerWindowByRole(const std::wstring& role, std::wstring& statusText)
{
    WorkerSnapshot snapshot = GetWorkerSnapshot(role);
    int artifactIndex = ArtifactIndexForRole(role);
    std::wstring displayName = artifactIndex >= 0 ? kControllerArtifacts[artifactIndex].displayName : role;

    if (!snapshot.running) {
        statusText = displayName + L" is not running";
        AddControllerHistory(L"Hide failed -> " + displayName + L" is not running", HideCommandDetail(role));
        return false;
    }
    if (!snapshot.window) {
        statusText = displayName + L" window not found";
        AddControllerHistory(L"Hide failed -> " + displayName + L" window not found", HideCommandDetail(role));
        return false;
    }

    ShowWindow(snapshot.window, SW_HIDE);
    RemoveRevealedWorkerRole(role);
    statusText = L"Hidden " + displayName;
    AddControllerHistory(L"Hide -> " + displayName + L" (" + role + L")", HideCommandDetail(role));

    std::ostringstream message;
    message << "[BgeControllerUi] hide role=" << Narrow(role) << " pid=" << snapshot.pid << " hwnd=" << reinterpret_cast<uintptr_t>(snapshot.window) << " result=hidden";
    LogRendererMessage(message.str());
    return true;
}

bool SendControllerCommandToWorker(const std::wstring& role, const std::wstring& commandText, std::wstring& statusText)
{
    WorkerSnapshot snapshot = GetWorkerSnapshot(role);
    int artifactIndex = ArtifactIndexForRole(role);
    std::wstring displayName = artifactIndex >= 0 ? kControllerArtifacts[artifactIndex].displayName : role;

    if (!snapshot.running) {
        statusText = displayName + L" is not running";
        AddControllerHistory(L"Command failed -> " + displayName + L" is not running: " + commandText, WorkerCommandDetail(role, commandText));
        return false;
    }
    if (!snapshot.window) {
        statusText = displayName + L" window not found";
        AddControllerHistory(L"Command failed -> " + displayName + L" window not found: " + commandText, WorkerCommandDetail(role, commandText));
        return false;
    }

    COPYDATASTRUCT copyData{};
    copyData.dwData = BGE_COPYDATA_WORKER_COMMAND;
    copyData.cbData = static_cast<DWORD>((commandText.size() + 1) * sizeof(wchar_t));
    copyData.lpData = const_cast<wchar_t*>(commandText.c_str());

    DWORD_PTR commandResult = 0;
    LRESULT deliveryResult = SendMessageTimeoutW(snapshot.window, WM_COPYDATA, reinterpret_cast<WPARAM>(g_hWnd), reinterpret_cast<LPARAM>(&copyData), SMTO_ABORTIFHUNG, 1000, &commandResult);
    bool delivered = deliveryResult != 0;
    bool executed = delivered && commandResult != 0;
    statusText = (executed ? L"Sent to " : L"Failed for ") + displayName;
    AddControllerHistory(std::wstring(executed ? L"Command OK -> " : L"Command failed -> ") + displayName + L": " + commandText, WorkerCommandDetail(role, commandText));

    std::ostringstream message;
    message << "[BgeControllerCommand] group=\"" << (artifactIndex >= 0 ? Narrow(kControllerArtifacts[artifactIndex].group) : "Unknown") << "\" target=" << Narrow(role)
        << " command=\"" << Narrow(commandText) << "\" delivery=" << (delivered ? "sent" : "failed") << " result=" << (executed ? "ok" : "failed");
    LogRendererMessage(message.str());
    return executed;
}

std::vector<std::wstring> SplitWorkerTelemetryPayload(const std::wstring& payload)
{
    std::vector<std::wstring> parts;
    std::wstring part;
    std::wstringstream stream(payload);
    while (std::getline(stream, part, L'\t')) {
        parts.push_back(part);
    }
    while (parts.size() < 4) {
        parts.push_back(L"");
    }
    return parts;
}

void SendWorkerTelemetry(const std::wstring& kind, const std::wstring& commandText, const std::wstring& statusText)
{
    if (g_isController || !g_sharedData || !EnsureCoordMutex()) {
        return;
    }

    DWORD controllerPid = 0;
    {
        CoordLock lock(g_coordMutex);
        if (!lock.locked()) {
            return;
        }
        controllerPid = g_sharedData->controllerPid;
    }

    if (controllerPid == 0 || controllerPid == g_currentPid || !IsProcessAlive(controllerPid)) {
        return;
    }

    HWND controllerWindow = FindWindowForProcess(controllerPid);
    if (!controllerWindow) {
        return;
    }

    std::wstring payload = g_workerName + L"\t" + kind + L"\t" + commandText + L"\t" + statusText;
    COPYDATASTRUCT copyData{};
    copyData.dwData = BGE_COPYDATA_WORKER_TELEMETRY;
    copyData.cbData = static_cast<DWORD>((payload.size() + 1) * sizeof(wchar_t));
    copyData.lpData = const_cast<wchar_t*>(payload.c_str());

    DWORD_PTR telemetryResult = 0;
    SendMessageTimeoutW(controllerWindow, WM_COPYDATA, reinterpret_cast<WPARAM>(g_hWnd), reinterpret_cast<LPARAM>(&copyData), SMTO_ABORTIFHUNG, 250, &telemetryResult);
}

bool HandleControllerTelemetryCopyData(COPYDATASTRUCT* copyData)
{
    if (!g_isController || !copyData || copyData->dwData != BGE_COPYDATA_WORKER_TELEMETRY || !copyData->lpData || copyData->cbData < sizeof(wchar_t)) {
        return false;
    }

    const wchar_t* payloadBuffer = static_cast<const wchar_t*>(copyData->lpData);
    std::vector<std::wstring> parts = SplitWorkerTelemetryPayload(payloadBuffer);
    std::wstring role = parts[0].empty() ? L"worker" : parts[0];
    std::wstring kind = parts[1].empty() ? L"event" : parts[1];
    std::wstring commandText = parts[2];
    std::wstring statusText = parts[3];

    std::wstring history = L"Telemetry <- " + role + L": " + kind;
    if (!commandText.empty()) {
        history += L" | " + commandText;
    }
    std::wstring detail = L"Worker telemetry\r\n  role: " + role
        + L"\r\n  kind: " + kind
        + L"\r\n  command: " + commandText
        + L"\r\n  status: " + statusText;
    AddControllerHistory(history, detail);
    SetCommandStatus(L"Telemetry: " + role + L" " + kind);
    return true;
}

void LogRuntimeSceneState()
{
    if (CurrentProcessOwnsGameLoop()) {
        std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> objectSlots;
        int selectedSlot = 0;
        bool animationRunning = false;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            objectSlots = g_objectSlots;
            selectedSlot = g_selectedObjectSlot;
            animationRunning = g_ballAnimationRunning;
        }
        BgeUpdateCollisionFlags(objectSlots);

        std::ostringstream header;
        header << "[BouncingBallCli] renderer=" << RendererApiName() << " selected-object=" << (selectedSlot + 1) << " animation=" << (animationRunning ? "running" : "stopped");
        LogRendererMessage(header.str());

        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            const BgeObjectSlotState& slot = objectSlots[index];
            if (!slot.visible) {
                continue;
            }
            std::ostringstream message;
            message << "[BouncingBallCli] object slot=" << (index + 1) << " x=" << slot.x << " y=" << slot.y << " vx=" << slot.velocityX << " vy=" << slot.velocityY
                << " collision=" << (slot.collisionDetected ? "yes" : "no") << " color=" << slot.colorR << "," << slot.colorG << "," << slot.colorB;
            LogRendererMessage(message.str());
        }
    }

    if (CurrentProcessOwnsSoundLoop()) {
        std::ostringstream message;
        message << "[SoundSlotCli] selected-sound-slot=" << (g_activeSoundSlot + 1);
        LogRendererMessage(message.str());
    }
}

void ApplyBallStateToRenderer()
{
    bool animationRunning = false;
    int selectedSlot = 0;
    bool objectSelectionActive = false;
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> objectSlots;
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> ghostSlots{};

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        PersistActiveObjectGroupLocked();
        animationRunning = g_ballAnimationRunning;
        selectedSlot = g_selectedObjectSlot;
        objectSelectionActive = g_objectSelectionActive;
        objectSlots = g_objectSlots;
        BgeUpdateCollisionFlags(objectSlots);
        if (g_ghostOverlayEnabled && g_ghostObjectGroupIndex >= 0 && g_ghostObjectGroupIndex < static_cast<int>(g_objectGroups.size()) && g_ghostObjectGroupIndex != g_activeObjectGroupIndex) {
            ghostSlots = g_objectGroups[static_cast<size_t>(g_ghostObjectGroupIndex)].slots;
        }
    }

    // Player runtime is the published-game shell. Authoring UI
    // (selection ring, vector arrow, slot labels) is for composing
    // the game in the controller and would be noise in the final
    // exe. Keep the underlying state intact; just hide the overlays.
    if (g_playerRuntimeMode) {
        objectSelectionActive = false;
    }
    // Also hide authoring overlays whenever the vector-ship game is live
    // (in-engine play). The selection ring + velocity-arrow obscure the
    // ship triangle and the arrow tracks velocity rather than heading,
    // so it doesn't rotate when the stationary ship turns - the exact
    // symptom Marc reported as "turn does nothing visually". Engine-neutral
    // rule: if a vector-ship player owns the selected slot, the player is
    // playing, not authoring - so the authoring chrome should get out of
    // the way.
    if (g_vectorShipActive && g_vectorShipState.active
        && g_vectorShipState.slotIndex >= 0
        && g_vectorShipState.slotIndex < BGE_OBJECT_SLOT_COUNT) {
        objectSelectionActive = false;
    }

    if (g_directX11Renderer) {
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            g_directX11Renderer->SetObjectSlotState(index, objectSlots[index]);
            g_directX11Renderer->SetGhostObjectSlotState(index, ghostSlots[index]);
        }
        g_directX11Renderer->SelectObjectSlot(selectedSlot);
        g_directX11Renderer->SetObjectSelectionActive(objectSelectionActive);
        g_directX11Renderer->SetAnimationRunning(animationRunning);
    }
    if (g_directX12Renderer) {
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            g_directX12Renderer->SetObjectSlotState(index, objectSlots[index]);
            g_directX12Renderer->SetGhostObjectSlotState(index, ghostSlots[index]);
        }
        g_directX12Renderer->SelectObjectSlot(selectedSlot);
        g_directX12Renderer->SetObjectSelectionActive(objectSelectionActive);
        g_directX12Renderer->SetAnimationRunning(animationRunning);
    }

    BgeTitleScreenState titleScreen;
    BgeScoreboardState scoreboard;
    std::vector<BgeCounterState> counters;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        titleScreen = g_titleScreenState;
        titleScreen.visible = titleScreen.visible && g_titleScreenActive;
        scoreboard = g_scoreboardState;
        scoreboard.visible = scoreboard.visible && g_scoreboardActive;
        counters = g_bgeCounters;
    }

    BgeGameViewport viewport = CurrentGameViewport();
    std::vector<BgeSceneOverlayText> overlays = BuildScoreboardOverlays(scoreboard, counters, viewport);
    std::vector<BgeSceneOverlayText> titleOverlays = BuildTitleScreenOverlays(titleScreen, viewport);
    overlays.insert(overlays.end(), titleOverlays.begin(), titleOverlays.end());
    ApplySceneOverlayTextToRenderer(overlays);
}

void LoadBackgroundOnActiveRenderer(const std::wstring& path)
{
    if (path.empty()) {
        return;
    }

    bool loaded = false;
    std::wstring error;
    if (g_rendererApi.load() == BgeRendererApi::DirectX12) {
        if (g_directX12Renderer) {
            loaded = g_directX12Renderer->LoadBackgroundImage(path);
            error = g_directX12Renderer->LastError();
        }
    }
    else if (g_directX11Renderer) {
        loaded = g_directX11Renderer->LoadBackgroundImage(path);
        error = g_directX11Renderer->LastError();
    }

    if (loaded) {
        LogRendererMessage(std::string("[BouncingBallControls] background-loaded path=") + Narrow(path));
    }
    else {
        LogRendererMessage(std::string("[BouncingBallControls] background-load-failed ") + Narrow(error));
    }
}

void InitializeSelectedRenderer(HWND hWnd)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return;
    }

    ShutdownActiveRenderer();

    if (g_rendererApi.load() == BgeRendererApi::DirectX12) {
        g_directX12Renderer = std::make_unique<DirectX12BouncingBallRenderer>();
        if (!g_directX12Renderer->Initialize(hWnd)) {
            LogRendererMessage("[DirectX12BouncingBallRenderer] " + Narrow(g_directX12Renderer->LastError()));
            g_directX12Renderer.reset();
            return;
        }
        ApplyBallStateToRenderer();
        LogRendererMessage("[DirectX12BouncingBallRenderer] initialized");
        return;
    }

    g_directX11Renderer = std::make_unique<DirectX11BouncingBallRenderer>();
    if (!g_directX11Renderer->Initialize(hWnd)) {
        LogRendererMessage("[DirectX11BouncingBallRenderer] " + Narrow(g_directX11Renderer->LastError()));
        g_directX11Renderer.reset();
        return;
    }
    ApplyBallStateToRenderer();
    LogRendererMessage("[DirectX11BouncingBallRenderer] initialized");
}

void ShutdownActiveRenderer()
{
    if (g_directX11Renderer) {
        g_directX11Renderer->Shutdown();
        g_directX11Renderer.reset();
    }
    if (g_directX12Renderer) {
        g_directX12Renderer->Shutdown();
        g_directX12Renderer.reset();
    }
}

void ResizeActiveRenderer()
{
    if (g_directX11Renderer) g_directX11Renderer->Resize();
    if (g_directX12Renderer) g_directX12Renderer->Resize();
}

void SyncObjectSlotsFromRenderer(const std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT>& objectSlots)
{
    std::lock_guard<std::mutex> lock(ballConfigMutex);
    if (!g_ballAnimationRunning) {
        return;
    }

    g_objectSlots = objectSlots;
    RefreshSelectedObjectGlobalsLocked();
    PersistActiveObjectGroupLocked();
}

void TickActiveRenderer(double deltaMilliseconds)
{
    if (g_rendererApi.load() == BgeRendererApi::DirectX12) {
        if (g_directX12Renderer) {
            g_directX12Renderer->Tick(deltaMilliseconds);
            SyncObjectSlotsFromRenderer(g_directX12Renderer->ObjectSlotStates());
            bool overlayActive = false;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                overlayActive = g_titleScreenActive || g_scoreboardActive;
            }
            // Evaluate every tick unconditionally — short-circuit `||` was
            // skipping TickBgeVectorShip whenever an earlier tick returned
            // true (e.g. projectiles in flight), which froze ship rotation
            // until bullets expired (perceived as "ship only turns when I
            // thrust"). Bitwise OR forces all ticks to run.
            bool asteroidDirty = TickAsteroidGameMode(deltaMilliseconds);
            bool projectileDirty = TickBgeProjectiles(deltaMilliseconds);
            bool ufoDirty = TickBgeUfo(deltaMilliseconds);
            bool shipDirty = TickBgeVectorShip(deltaMilliseconds);
            if (asteroidDirty | projectileDirty | ufoDirty | shipDirty | overlayActive) {
                ApplyBallStateToRenderer();
            }
        }
        return;
    }
    if (g_directX11Renderer) {
        g_directX11Renderer->Tick(deltaMilliseconds);
        SyncObjectSlotsFromRenderer(g_directX11Renderer->ObjectSlotStates());
        bool overlayActive = false;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            overlayActive = g_titleScreenActive || g_scoreboardActive;
        }
        // See DX12 branch above for the short-circuit explanation.
        bool asteroidDirty = TickAsteroidGameMode(deltaMilliseconds);
        bool projectileDirty = TickBgeProjectiles(deltaMilliseconds);
        bool ufoDirty = TickBgeUfo(deltaMilliseconds);
        bool shipDirty = TickBgeVectorShip(deltaMilliseconds);
        if (asteroidDirty | projectileDirty | ufoDirty | shipDirty | overlayActive) {
            ApplyBallStateToRenderer();
        }
    }
}

void RenderActiveRenderer()
{
    if (g_rendererApi.load() == BgeRendererApi::DirectX12) {
        if (g_directX12Renderer) g_directX12Renderer->Render();
        return;
    }
    if (g_directX11Renderer) g_directX11Renderer->Render();
}

float ReadFloatControl(HWND control, float fallback)
{
    wchar_t text[64]{};
    if (!control || GetWindowTextW(control, text, static_cast<int>(std::size(text))) == 0) {
        return fallback;
    }
    wchar_t* parseEnd = nullptr;
    float value = static_cast<float>(std::wcstod(text, &parseEnd));
    return parseEnd == text ? fallback : value;
}

float ReadColorControl(HWND control, float fallback)
{
    float value = ReadFloatControl(control, fallback * 255.0f);
    value = (std::max)(0.0f, (std::min)(255.0f, value));
    return value / 255.0f;
}

void AddBallFromControls()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return;
    }

    bool movedToNextEmptySlot = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_objectSlots[g_selectedObjectSlot].visible) {
            int nextSlot = FirstHiddenObjectSlotLocked();
            if (nextSlot >= 0) {
                SelectObjectSlotStateLocked(nextSlot);
                movedToNextEmptySlot = true;
            }
        }
    }

    if (!movedToNextEmptySlot) {
        ApplyVectorFromControls();
        ApplyColorFromControls();
    }

    int addedSlot = 0;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        AddObjectSlotStateLocked(g_selectedObjectSlot);
        addedSlot = g_selectedObjectSlot;
        g_objectSlots[addedSlot].shape = BgeObjectShape::Ball;
        g_objectSlots[addedSlot].kind = BgeObjectKind::Generic;
        g_objectSlots[addedSlot].colorA = 1.0f;
        RefreshSelectedObjectGlobalsLocked();
        g_rendererStateDirty = true;
    }
    SyncBallControls();
    std::ostringstream message;
    message << "[BouncingBallControls] add-ball slot=" << (addedSlot + 1) << " renderer=" << RendererApiName();
    LogRendererMessage(message.str());
}

void StartAnimationFromControls()
{
    std::wstring statusText;
    StartAnimationState(statusText);
    SetCommandStatus(statusText);
}

void StopAnimationFromControls()
{
    std::wstring statusText;
    StopAnimationState(statusText);
    SetCommandStatus(statusText);
}

void ApplyVectorFromControls()
{
    float velocityX = ReadFloatControl(g_velocityXEdit, g_ballVelocityX);
    float velocityY = ReadFloatControl(g_velocityYEdit, g_ballVelocityY);
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        ApplyObjectVectorStateLocked(velocityX, velocityY);
    }
    std::ostringstream message;
    message << "[BouncingBallControls] set-vector slot=" << (g_selectedObjectSlot + 1) << " vx=" << velocityX << " vy=" << velocityY;
    LogRendererMessage(message.str());
}

void ApplyColorFromControls()
{
    float colorR = ReadColorControl(g_colorREdit, g_ballColorR);
    float colorG = ReadColorControl(g_colorGEdit, g_ballColorG);
    float colorB = ReadColorControl(g_colorBEdit, g_ballColorB);
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        ApplyObjectColorStateLocked(colorR, colorG, colorB);
    }
    std::ostringstream message;
    message << "[BouncingBallControls] set-color slot=" << (g_selectedObjectSlot + 1) << " r=" << colorR << " g=" << colorG << " b=" << colorB;
    LogRendererMessage(message.str());
}

void SwitchRendererFromControls()
{
    if (!g_rendererCombo || !CurrentProcessOwnsGameLoop()) {
        return;
    }

    LRESULT selection = SendMessageW(g_rendererCombo, CB_GETCURSEL, 0, 0);
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        g_rendererApi.store(selection == 1 ? BgeRendererApi::DirectX12 : BgeRendererApi::DirectX11);
        g_rendererSwitchRequested = true;
        g_rendererStateDirty = true;
    }
    LogRendererMessage(std::string("[BouncingBallControls] switch-renderer renderer=") + RendererApiName());
}

void ProcessPendingRendererCommands()
{
    bool switchRequested = false;
    bool stateDirty = false;
    bool resizeRequested = false;
    bool backgroundImageDirty = false;
    std::wstring backgroundImagePath;

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        switchRequested = g_rendererSwitchRequested;
        stateDirty = g_rendererStateDirty;
        resizeRequested = g_rendererResizeRequested;
        backgroundImageDirty = g_backgroundImageDirty;
        backgroundImagePath = g_backgroundImagePath;
        g_rendererSwitchRequested = false;
        g_rendererStateDirty = false;
        g_rendererResizeRequested = false;
        g_backgroundImageDirty = false;
    }

    if (switchRequested) {
        InitializeSelectedRenderer(g_hWnd);
    }
    if (stateDirty || switchRequested) {
        ApplyBallStateToRenderer();
    }
    if ((backgroundImageDirty || switchRequested) && !backgroundImagePath.empty()) {
        LoadBackgroundOnActiveRenderer(backgroundImagePath);
    }
    if (resizeRequested) {
        ResizeActiveRenderer();
    }
}

void SelectObjectSlotFromControls(int slotIndex)
{
    if (!CurrentProcessOwnsGameLoop() || slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_objectSlots[slotIndex].isDeleted) {
            SetCommandStatus(L"Object is deleted; use Ctrl+Z to restore it");
            return;
        }
        SelectObjectSlotStateLocked(slotIndex);
    }

    SyncBallControls();
    std::ostringstream message;
    message << "[BouncingBallControls] select-object-slot slot=" << (slotIndex + 1);
    LogRendererMessage(message.str());
}

void SelectSoundSlotFromControls(int slotIndex)
{
    if (!CurrentProcessOwnsSoundLoop() || slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
        return;
    }

    g_activeSoundSlot = slotIndex;
    SyncBallControls();
    std::ostringstream message;
    message << "[SoundSlotControls] select-sound-slot slot=" << (slotIndex + 1);
    LogRendererMessage(message.str());
}

void AdvanceSoundSlotLoop()
{
    if (!CurrentProcessOwnsSoundLoop()) {
        return;
    }

    DWORD64 now = GetTickCount64();
    if (now - g_lastSoundSlotTick < 1000) {
        return;
    }
    g_lastSoundSlotTick = now;
    g_activeSoundSlot = (g_activeSoundSlot + 1) % BGE_OBJECT_SLOT_COUNT;
    SyncBallControls();
    std::ostringstream message;
    message << "[SoundSlotControls] loop-sound-slot slot=" << (g_activeSoundSlot + 1);
    LogRendererMessage(message.str());
}

std::vector<std::wstring> TokenizeCommandText(const std::wstring& commandText)
{
    std::wstring wrapped = L"bgecmd " + commandText;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(wrapped.c_str(), &argc);
    std::vector<std::wstring> tokens;
    if (!argv) {
        return tokens;
    }

    for (int index = 1; index < argc; ++index) {
        tokens.push_back(argv[index]);
    }
    LocalFree(argv);
    return tokens;
}

std::wstring JoinCommandTokens(const std::vector<std::wstring>& tokens, size_t firstIndex)
{
    std::wstring result;
    for (size_t index = firstIndex; index < tokens.size(); ++index) {
        if (!result.empty()) {
            result += L" ";
        }
        result += tokens[index];
    }
    return result;
}

void SetCommandStatus(const std::wstring& statusText)
{
    if (g_commandStatus) {
        SetWindowTextW(g_commandStatus, statusText.c_str());
    }
}

bool ExecuteControllerCommandText(const std::wstring& commandText, std::wstring& statusText)
{
    std::vector<std::wstring> tokens = TokenizeCommandText(commandText);
    if (tokens.empty()) {
        statusText = L"Enter a command";
        return false;
    }

    AddControllerHistory(L"Input -> " + commandText, ControllerCommandDetail(commandText));

    std::wstring command = LowerArg(tokens[0]);
    if (command == L"help" || command == L"?") {
        statusText = L"game load <file> | target <group|worker> | inspect | hide | launch | history | worker: command";
        return true;
    }

    if (command == L"history") {
        ShowControllerHistoryWindow();
        statusText = L"History window open";
        return true;
    }

    if (command == L"game" && tokens.size() >= 2 && (LowerArg(tokens[1]) == L"load" || LowerArg(tokens[1]) == L"import")) {
        if (tokens.size() < 3) {
            statusText = L"Use: game load <json|csv|command-file>";
            return false;
        }
        return QueueConstructionArtifactCommandsFromFile(JoinCommandTokens(tokens, 2), statusText);
    }

    if (command == L"load" && tokens.size() >= 2 && LowerArg(tokens[1]) == L"game") {
        if (tokens.size() < 3) {
            statusText = L"Use: load game <json|csv|command-file>";
            return false;
        }
        return QueueConstructionArtifactCommandsFromFile(JoinCommandTokens(tokens, 2), statusText);
    }

    if (command == L"asteroid-game" || command == L"asteroids") {
        // NOTE: do NOT alias bare "game" here — that would collide with the
        // recipe's first command "game define <id> title <name> version <n>"
        // and trigger an infinite recipe re-queue loop (recipe pushes
        // commands; first command "game define" matches "game" alias;
        // LoadAsteroidGameFromController re-pushes the whole recipe;
        // every tick the rock-field/title-screen/player-ship commands
        // re-run, asteroids snap to starting positions, title overlay
        // never dismisses).
        LoadAsteroidGameFromController();
        statusText = L"Asteroid Game queued";
        return true;
    }

    if (command == L"target" || command == L"group") {
        if (tokens.size() < 2) {
            statusText = L"Use: target <group|worker>";
            return false;
        }
        int targetIndex = ResolveControllerArtifactIndex(JoinCommandTokens(tokens, 1));
        if (targetIndex < 0) {
            statusText = L"Target not found";
            return false;
        }
        SelectControllerArtifact(targetIndex);
        statusText = L"Target: " + std::wstring(kControllerArtifacts[targetIndex].displayName);
        return true;
    }

    if (command == L"launch") {
        if (tokens.size() >= 2 && LowerArg(tokens[1]) == L"stack") {
            LaunchBasicGameStack();
            statusText = L"Launch requested for stack";
            return true;
        }
        int targetIndex = tokens.size() >= 2 ? ResolveControllerArtifactIndex(JoinCommandTokens(tokens, 1)) : g_selectedControllerArtifact;
        if (targetIndex < 0) {
            statusText = L"Launch target not found";
            return false;
        }
        LaunchControllerArtifact(targetIndex);
        statusText = L"Launch requested for " + std::wstring(kControllerArtifacts[targetIndex].displayName);
        return true;
    }

    if (command == L"inspect" || command == L"show") {
        int targetIndex = tokens.size() >= 2 ? ResolveControllerArtifactIndex(JoinCommandTokens(tokens, 1)) : g_selectedControllerArtifact;
        if (targetIndex < 0) {
            statusText = L"Inspect target not found";
            return false;
        }
        SelectControllerArtifact(targetIndex);
        return InspectWorkerWindowByRole(kControllerArtifacts[targetIndex].role, statusText);
    }

    if (command == L"hide") {
        int targetIndex = tokens.size() >= 2 ? ResolveControllerArtifactIndex(JoinCommandTokens(tokens, 1)) : g_selectedControllerArtifact;
        if (targetIndex < 0) {
            statusText = L"Hide target not found";
            return false;
        }
        SelectControllerArtifact(targetIndex);
        return HideWorkerWindowByRole(kControllerArtifacts[targetIndex].role, statusText);
    }

    std::wstring targetRole = SelectedControllerRole();
    std::wstring routedCommand = commandText;
    size_t colon = commandText.find(L':');
    if (colon != std::wstring::npos) {
        std::wstring selector = TrimText(commandText.substr(0, colon));
        int targetIndex = ResolveControllerArtifactIndex(selector);
        if (targetIndex < 0) {
            statusText = L"Target not found";
            return false;
        }
        SelectControllerArtifact(targetIndex);
        targetRole = kControllerArtifacts[targetIndex].role;
        routedCommand = TrimText(commandText.substr(colon + 1));
    }

    if (targetRole.empty()) {
        statusText = L"No command target";
        return false;
    }
    if (routedCommand.empty()) {
        statusText = L"Enter a worker command";
        return false;
    }
    return SendControllerCommandToWorker(targetRole, routedCommand, statusText);
}

void ExecuteCommandBarInput()
{
    if (!g_commandEdit) {
        return;
    }

    wchar_t commandText[512]{};
    GetWindowTextW(g_commandEdit, commandText, static_cast<int>(std::size(commandText)));
    std::wstring statusText;
    if (ExecuteCommandText(commandText, statusText)) {
        RecordWorkerCommandHistory(commandText);
        SendWorkerTelemetry(L"command", commandText, statusText);
        SetWindowTextW(g_commandEdit, L"");
    }
    else {
        SendWorkerTelemetry(L"command-failed", commandText, statusText);
    }
    SetCommandStatus(statusText);
}

bool ExecuteCommandText(const std::wstring& commandText, std::wstring& statusText)
{
    if (g_isController) {
        return ExecuteControllerCommandText(commandText, statusText);
    }

    std::vector<std::wstring> tokens = TokenizeCommandText(commandText);
    if (tokens.empty()) {
        statusText = L"Enter a command";
        return false;
    }

    std::wstring command = LowerArg(tokens[0]);
    auto logCommand = [&commandText](const std::string& result) {
        LogRendererMessage(std::string("[BgeCommandBar] ") + result + " command=\"" + Narrow(commandText) + "\"");
    };

    if (command == L"help" || command == L"?") {
        statusText = L"plugin import/import-set | player-ship create | projectile create | ufo create | counter define/set | scoreboard create | title-screen create | export executable | inspect commands | mapping";
        logCommand("help");
        return true;
    }

    if (command == L"plugin" || command == L"capability") {
        bool ok = ExecuteBgePluginCommand(tokens, statusText);
        logCommand(ok ? "plugin" : "plugin failed");
        return ok;
    }

    if (command == L"inspect") {
        bool ok = ExecuteBgeInspectCommand(tokens, statusText);
        logCommand(ok ? "inspect" : "inspect failed");
        return ok;
    }

    if (command == L"export") {
        bool ok = ExecuteBgeExportCommand(tokens, statusText);
        logCommand(ok ? "export" : "export failed");
        return ok;
    }

    if (command == L"viewport") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"viewport commands run in bge.game-loop";
            return false;
        }
        if (HasCommandFlag(tokens, L"--wrap-x") || HasCommandFlag(tokens, L"--wrap-y") || HasCommandFlag(tokens, L"--wrap")) {
            BgeSetCurrentEdgePolicy(BgeEdgePolicy::Wrap);
        }
        statusText = g_playerRuntimeMode ? L"Viewport bound to player window" : L"Viewport configured for game-loop window";
        logCommand("viewport");
        return true;
    }

    if (command == L"screen") {
        statusText = L"Screen shell command accepted";
        logCommand("screen");
        return true;
    }

    if (command == L"title-screen") {
        bool ok = ExecuteBgeTitleScreenCommand(tokens, statusText);
        logCommand(ok ? "title-screen" : "title-screen failed");
        return ok;
    }

    if (command == L"counter" || command == L"counters") {
        bool ok = ExecuteBgeCounterCommand(tokens, statusText);
        logCommand(ok ? "counter" : "counter failed");
        return ok;
    }

    if (command == L"scoreboard") {
        bool ok = ExecuteBgeScoreboardCommand(tokens, statusText);
        logCommand(ok ? "scoreboard" : "scoreboard failed");
        return ok;
    }

    if (command == L"rock-field" || command == L"breakable-rock-field") {
        bool ok = ExecuteBgeBreakableRockFieldCommand(tokens, statusText);
        logCommand(ok ? "rock-field" : "rock-field failed");
        return ok;
    }

    if (command == L"projectile") {
        bool ok = ExecuteBgeProjectileCommand(tokens, statusText);
        logCommand(ok ? "projectile" : "projectile failed");
        return ok;
    }

    if (command == L"ufo" || command == L"saucer") {
        bool ok = ExecuteBgeUfoCommand(tokens, statusText);
        logCommand(ok ? "ufo" : "ufo failed");
        return ok;
    }

    if (command == L"player-ship" || command == L"vector-ship") {
        bool ok = ExecuteBgeVectorShipCommand(tokens, statusText);
        logCommand(ok ? "player-ship" : "player-ship failed");
        return ok;
    }

    if (command == L"sound") {
        bool ok = ExecuteBgeSoundCommand(tokens, statusText);
        logCommand(ok ? "sound" : "sound failed");
        return ok;
    }

    if (command == L"game" && tokens.size() >= 2) {
        std::wstring subcommand = LowerArg(tokens[1]);
        if (subcommand == L"define" || subcommand == L"start" || subcommand == L"status") {
            if (subcommand == L"define") {
                std::wstring value;
                if (TryGetCommandOptionValue(tokens, L"--name", value) && !value.empty()) {
                    g_playerRuntimeTitle = NormalizeTitleScreenText(value);
                    if (g_playerRuntimeMode && g_hWnd) {
                        SetWindowTextW(g_hWnd, g_playerRuntimeTitle.c_str());
                    }
                }
            }
            if (subcommand == L"start") {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                g_ballAnimationRunning = true;
                g_rendererStateDirty = true;
            }
            statusText = L"Game shell: " + subcommand;
            logCommand("game-shell");
            return true;
        }
    }

    if (command == L"asteroid-game" || command == L"asteroids" || command == L"game") {
        bool ok = ExecuteAsteroidGameModuleCommand(tokens, statusText);
        logCommand(ok ? "asteroid-game" : "asteroid-game failed");
        return ok;
    }

    if (command == L"mapping" || command == L"controls" || command == L"keys") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Mapping commands run in bge.game-loop";
            return false;
        }
        ShowMappingWindow();
        logCommand("show-mapping");
        statusText = L"Mapping window open";
        return true;
    }

    if (command == L"mode") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Mode commands run in bge.game-loop";
            return false;
        }
        if (tokens.size() < 2) {
            statusText = L"Mode: " + EditModeName(g_editMode);
            logCommand("mode-current");
            return true;
        }
        std::wstring modeArg = LowerArg(tokens[1]);
        if (modeArg == L"next" || modeArg == L"up") {
            CycleEditMode(1);
        }
        else if (modeArg == L"prev" || modeArg == L"previous" || modeArg == L"down") {
            CycleEditMode(-1);
        }
        else if (!SetEditModeFromText(modeArg)) {
            statusText = L"Use: mode translate|resize|rotate";
            return false;
        }
        logCommand("mode=" + Narrow(EditModeName(g_editMode)));
        statusText = L"Mode: " + EditModeName(g_editMode);
        return true;
    }

    if (command == L"rate" || command == L"edit-rate") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Rate commands run in bge.game-loop";
            return false;
        }
        if (tokens.size() < 2) {
            statusText = L"Rate: " + EditRateLabel();
            logCommand("rate-current");
            return true;
        }
        if (!SetEditRateFromText(tokens[1])) {
            statusText = L"Use: rate 0.25x|0.5x|1x|2x|4x|up|down";
            return false;
        }
        logCommand("rate=" + Narrow(EditRateLabel()));
        statusText = L"Rate: " + EditRateLabel();
        return true;
    }

    if (command == L"translate" || command == L"move") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Translate commands run in bge.game-loop";
            return false;
        }
        if (tokens.size() < 3) {
            statusText = L"Use: translate dx dy";
            return false;
        }
        float deltaX = 0.0f;
        float deltaY = 0.0f;
        if (!TryParseFloatArg(tokens[1], deltaX) || !TryParseFloatArg(tokens[2], deltaY)) {
            statusText = L"Translate values must be numbers";
            return false;
        }
        bool ok = TranslateSelectedObject(deltaX, deltaY, statusText);
        logCommand(ok ? "translate" : "translate failed");
        return ok;
    }

    if (command == L"resize" || command == L"scale") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Resize commands run in bge.game-loop";
            return false;
        }
        if (tokens.size() < 2) {
            statusText = L"Use: resize delta-radius";
            return false;
        }
        float deltaRadius = 0.0f;
        if (!TryParseFloatArg(tokens[1], deltaRadius)) {
            statusText = L"Resize value must be a number";
            return false;
        }
        bool ok = ResizeSelectedObject(deltaRadius, statusText);
        logCommand(ok ? "resize" : "resize failed");
        return ok;
    }

    if (command == L"rotate" || command == L"turn") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Rotate commands run in bge.game-loop";
            return false;
        }
        if (tokens.size() < 2) {
            statusText = L"Use: rotate degrees";
            return false;
        }
        float deltaDegrees = 0.0f;
        if (!TryParseFloatArg(tokens[1], deltaDegrees)) {
            statusText = L"Rotate value must be a number";
            return false;
        }
        bool ok = RotateSelectedObject(deltaDegrees, statusText);
        logCommand(ok ? "rotate" : "rotate failed");
        return ok;
    }

    if (command == L"undo" || command == L"ctrl-z") {
        bool ok = UndoLastDeleteAction(statusText);
        logCommand(ok ? "undo-delete" : "undo-delete failed");
        return ok;
    }

    if (command == L"delete" || command == L"del") {
        std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"auto";
        bool ok = false;
        if (subcommand == L"commit" || subcommand == L"final") {
            ok = CommitMarkedObjectDeletes(statusText);
        }
        else if (subcommand == L"mark" || subcommand == L"unmark" || subcommand == L"toggle" || subcommand == L"auto") {
            bool selectionActive = false;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                selectionActive = g_objectSelectionActive;
            }
            ok = selectionActive ? ToggleSelectedObjectDeleteMark(statusText) : CommitMarkedObjectDeletes(statusText);
        }
        else {
            statusText = L"Use: delete [mark|commit]";
            return false;
        }
        logCommand(ok ? "delete" : "delete failed");
        return ok;
    }

    if (command == L"group" || command == L"object-group") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Group commands run in bge.game-loop";
            return false;
        }

        std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"current";
        if (subcommand == L"list") {
            std::wstring listText;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                PersistActiveObjectGroupLocked();
                for (int groupIndex = 0; groupIndex < static_cast<int>(g_objectGroups.size()); ++groupIndex) {
                    if (!listText.empty()) {
                        listText += L", ";
                    }
                    listText += std::to_wstring(groupIndex + 1) + L":" + g_objectGroups[static_cast<size_t>(groupIndex)].label;
                    if (groupIndex == g_activeObjectGroupIndex) {
                        listText += L"*";
                    }
                }
            }
            statusText = listText.empty() ? L"No groups" : listText;
            logCommand("group-list");
            return true;
        }

        if (subcommand == L"add" || subcommand == L"new" || subcommand == L"+") {
            int groupIndex = -1;
            std::wstring groupLabel;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                if (g_ballAnimationRunning) {
                    statusText = L"Stop animation before adding a group";
                    return false;
                }
                groupIndex = AddObjectGroupStateLocked(tokens.size() >= 3 ? JoinCommandTokens(tokens, 2) : L"");
                groupLabel = ObjectGroupLabel(groupIndex);
            }
            SyncBallControls();
            std::ostringstream message;
            message << "group-add index=" << (groupIndex + 1) << " label=" << Narrow(groupLabel);
            logCommand(message.str());
            statusText = L"Added group: " + groupLabel;
            return true;
        }

        if (subcommand == L"select" || subcommand == L"use" || subcommand == L"switch") {
            if (tokens.size() < 3) {
                statusText = L"Use: group select <name|number>";
                return false;
            }
            std::wstring selector = JoinCommandTokens(tokens, 2);
            int groupIndex = ResolveObjectGroupIndex(selector);
            if (groupIndex < 0) {
                statusText = L"Group not found";
                return false;
            }
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                if (g_ballAnimationRunning) {
                    statusText = L"Stop animation before switching groups";
                    return false;
                }
                SelectObjectGroupStateLocked(groupIndex);
            }
            SyncBallControls();
            logCommand("group-select");
            statusText = L"Group: " + ObjectGroupLabel(groupIndex);
            return true;
        }

        statusText = L"Group: " + ObjectGroupLabel(g_activeObjectGroupIndex);
        logCommand("group-current");
        return true;
    }

    if (command == L"ghost") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Ghost commands run in bge.game-loop";
            return false;
        }

        std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
        if (subcommand == L"off" || subcommand == L"clear" || subcommand == L"none") {
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                g_ghostOverlayEnabled = false;
                g_ghostObjectGroupIndex = -1;
                g_rendererStateDirty = true;
            }
            SyncBallControls();
            logCommand("ghost-off");
            statusText = L"Ghost off";
            return true;
        }

        if (subcommand == L"on") {
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                if (g_ghostObjectGroupIndex < 0 || g_ghostObjectGroupIndex == g_activeObjectGroupIndex) {
                    g_ghostObjectGroupIndex = -1;
                    for (int groupIndex = 0; groupIndex < static_cast<int>(g_objectGroups.size()); ++groupIndex) {
                        if (groupIndex != g_activeObjectGroupIndex) {
                            g_ghostObjectGroupIndex = groupIndex;
                            break;
                        }
                    }
                }
                if (g_ghostObjectGroupIndex < 0) {
                    statusText = L"Add another group before ghosting";
                    return false;
                }
                g_ghostOverlayEnabled = true;
                g_rendererStateDirty = true;
                statusText = L"Ghosting: " + ObjectGroupLabel(g_ghostObjectGroupIndex);
            }
            SyncBallControls();
            logCommand("ghost-on");
            return true;
        }

        if (subcommand == L"group" || subcommand == L"select" || subcommand == L"use") {
            if (tokens.size() < 3) {
                statusText = L"Use: ghost group <name|number>";
                return false;
            }
            int groupIndex = ResolveObjectGroupIndex(JoinCommandTokens(tokens, 2));
            if (groupIndex < 0) {
                statusText = L"Ghost group not found";
                return false;
            }
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                g_ghostObjectGroupIndex = groupIndex;
                g_ghostOverlayEnabled = groupIndex != g_activeObjectGroupIndex;
                g_rendererStateDirty = true;
                statusText = g_ghostOverlayEnabled ? L"Ghosting: " + ObjectGroupLabel(groupIndex) : L"Ghost group matches active group";
            }
            SyncBallControls();
            logCommand("ghost-group");
            return true;
        }

        statusText = g_ghostOverlayEnabled && g_ghostObjectGroupIndex >= 0 ? L"Ghosting: " + ObjectGroupLabel(g_ghostObjectGroupIndex) : L"Ghost off";
        logCommand("ghost-status");
        return true;
    }

    if (command == L"player" || command == L"main-player") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Player commands run in bge.game-loop";
            return false;
        }

        std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
        if (subcommand == L"clear" || subcommand == L"none") {
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                g_mainPlayerGroupIndex = -1;
                g_mainPlayerSlot = -1;
            }
            SyncBallControls();
            logCommand("player-clear");
            statusText = L"Player cleared";
            return true;
        }

        if (subcommand == L"set" || subcommand == L"select") {
            int groupIndex = g_activeObjectGroupIndex;
            int slotIndex = g_selectedObjectSlot;
            if (tokens.size() >= 3) {
                int parsedSlot = -1;
                if (TryParseSlotArg(tokens.back(), parsedSlot)) {
                    slotIndex = parsedSlot;
                    if (tokens.size() > 3) {
                        groupIndex = ResolveObjectGroupIndex(JoinCommandTokens(std::vector<std::wstring>(tokens.begin(), tokens.end() - 1), 2));
                    }
                }
                else {
                    statusText = L"Use: player set [group] <1-10>";
                    return false;
                }
            }

            if (groupIndex < 0) {
                statusText = L"Player group not found";
                return false;
            }

            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                PersistActiveObjectGroupLocked();
                if (groupIndex >= static_cast<int>(g_objectGroups.size()) ||
                    !g_objectGroups[static_cast<size_t>(groupIndex)].slots[slotIndex].visible ||
                    g_objectGroups[static_cast<size_t>(groupIndex)].slots[slotIndex].isDeleted) {
                    statusText = L"Player object must be visible";
                    return false;
                }
                g_mainPlayerGroupIndex = groupIndex;
                g_mainPlayerSlot = slotIndex;
            }
            SyncBallControls();
            logCommand("player-set");
            statusText = L"Main player: " + ObjectGroupLabel(groupIndex) + L" / object " + std::to_wstring(slotIndex + 1);
            return true;
        }

        statusText = g_mainPlayerGroupIndex >= 0 && g_mainPlayerSlot >= 0
            ? L"Main player: " + ObjectGroupLabel(g_mainPlayerGroupIndex) + L" / object " + std::to_wstring(g_mainPlayerSlot + 1)
            : L"Player: none";
        logCommand("player-status");
        return true;
    }

    if (command == L"edge" || command == L"edge-policy") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Edge commands run in bge.game-loop";
            return false;
        }

        if (tokens.size() < 2 || LowerArg(tokens[1]) == L"status") {
            BgeEdgePolicy current = BgeCurrentEdgePolicy();
            statusText = std::wstring(L"Edge policy: ") + BgeEdgePolicyName(current);
            logCommand(std::string("edge-status=") + Narrow(BgeEdgePolicyName(current)));
            return true;
        }

        std::wstring policyArg = LowerArg(tokens[1]);
        BgeEdgePolicy parsed = BgeEdgePolicy::Bounce;
        if (!BgeTryParseEdgePolicy(policyArg, parsed)) {
            statusText = L"Use: edge bounce|wrap|clamp|status";
            return false;
        }

        BgeSetCurrentEdgePolicy(parsed);
        logCommand(std::string("edge=") + Narrow(BgeEdgePolicyName(parsed)));
        statusText = std::wstring(L"Edge policy: ") + BgeEdgePolicyName(parsed);
        return true;
    }

    if (command == L"score" || command == L"lives" || command == L"fire" || command == L"restart" || command == L"pause" || command == L"resume" || command == L"hyperspace" || command == L"hyper") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Asteroid Game component commands run in bge.game-loop";
            return false;
        }
        bool ok = ExecuteAsteroidGameModuleCommand(tokens, statusText);
        logCommand(ok ? "asteroid-component" : "asteroid-component failed");
        return ok;
    }

    if (command == L"asteroid" || command == L"rock") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Asteroid commands run in bge.game-loop";
            return false;
        }

        std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"status";
        if (subcommand == L"game" || subcommand == L"play" || subcommand == L"start" || subcommand == L"status" || subcommand == L"state"
            || subcommand == L"restart" || subcommand == L"reset" || subcommand == L"score" || subcommand == L"lives" || subcommand == L"fire"
            || subcommand == L"pause" || subcommand == L"resume" || subcommand == L"hyperspace" || subcommand == L"hyper"
            || subcommand == L"player" || subcommand == L"add" || subcommand == L"spawn" || subcommand == L"target" || subcommand == L"remove"
            || subcommand == L"clear" || subcommand == L"count" || subcommand == L"hud" || subcommand == L"header" || subcommand == L"title"
            || subcommand == L"screen" || subcommand == L"commands" || subcommand == L"help") {
            bool ok = ExecuteAsteroidGameModuleCommand(tokens, statusText);
            logCommand(ok ? "asteroid-module-command" : "asteroid-module-command failed");
            return ok;
        }

        if (subcommand == L"window" || subcommand == L"controls" || subcommand == L"alpha-window") {
            ShowAsteroidAlphaWindow();
            logCommand("asteroid-window");
            statusText = L"Asteroid Alpha window open";
            return true;
        }

        if (subcommand == L"set" || subcommand == L"shape") {
            int slotIndex = -1;
            if (tokens.size() >= 3) {
                TryParseSlotArg(tokens[2], slotIndex);
            }
            int selectedSlot = 0;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                if (slotIndex >= 0) {
                    SelectObjectSlotStateLocked(slotIndex);
                }
                ApplyObjectAsteroidStateLocked(g_ballColorR, g_ballColorG, g_ballColorB, g_ballColorA, true);
                selectedSlot = g_selectedObjectSlot;
            }
            SyncBallControls();
            std::ostringstream message;
            message << "asteroid-set slot=" << (selectedSlot + 1);
            logCommand(message.str());
            statusText = L"Asteroid object " + std::to_wstring(selectedSlot + 1);
            return true;
        }

        if (subcommand == L"ball" || subcommand == L"circle") {
            int selectedSlot = 0;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                ApplyObjectShapeStateLocked(BgeObjectShape::Ball);
                g_objectSlots[g_selectedObjectSlot].kind = BgeObjectKind::Generic;
                PersistActiveObjectGroupLocked();
                selectedSlot = g_selectedObjectSlot;
            }
            SyncBallControls();
            logCommand("asteroid-shape-ball");
            statusText = L"Object " + std::to_wstring(selectedSlot + 1) + L" shape: ball";
            return true;
        }

        if (subcommand == L"alpha" || subcommand == L"a") {
            if (tokens.size() < 3) {
                statusText = L"Use: asteroid alpha 0-255";
                return false;
            }
            float alpha = 0.0f;
            if (!TryParseFloatArg(tokens[2], alpha)) {
                statusText = L"Asteroid alpha must be a number";
                return false;
            }
            int selectedSlot = 0;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                ApplyObjectAlphaStateLocked(NormalizeColorArg(alpha));
                ApplyObjectShapeStateLocked(BgeObjectShape::Asteroid);
                g_objectSlots[g_selectedObjectSlot].kind = BgeObjectKind::Asteroid;
                PersistActiveObjectGroupLocked();
                selectedSlot = g_selectedObjectSlot;
            }
            SyncBallControls();
            std::ostringstream message;
            message << "asteroid-alpha slot=" << (selectedSlot + 1);
            logCommand(message.str());
            statusText = L"Asteroid alpha set for object " + std::to_wstring(selectedSlot + 1);
            return true;
        }

        if (subcommand == L"color" || subcommand == L"rgba") {
            size_t valueIndex = 2;
            int slotIndex = -1;
            if (tokens.size() >= 6 && TryParseSlotArg(tokens[valueIndex], slotIndex)) {
                ++valueIndex;
            }
            if (valueIndex + 2 >= tokens.size()) {
                statusText = L"Use: asteroid color [slot] r g b [a]";
                return false;
            }
            float colorR = 0.0f;
            float colorG = 0.0f;
            float colorB = 0.0f;
            float alpha = g_ballColorA;
            if (!TryParseFloatArg(tokens[valueIndex], colorR) || !TryParseFloatArg(tokens[valueIndex + 1], colorG) || !TryParseFloatArg(tokens[valueIndex + 2], colorB)) {
                statusText = L"Asteroid color values must be numbers";
                return false;
            }
            if (valueIndex + 3 < tokens.size() && !TryParseFloatArg(tokens[valueIndex + 3], alpha)) {
                statusText = L"Asteroid alpha must be a number";
                return false;
            }
            int selectedSlot = 0;
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                if (slotIndex >= 0) {
                    SelectObjectSlotStateLocked(slotIndex);
                }
                ApplyObjectAsteroidStateLocked(NormalizeColorArg(colorR), NormalizeColorArg(colorG), NormalizeColorArg(colorB), NormalizeColorArg(alpha), false);
                selectedSlot = g_selectedObjectSlot;
            }
            SyncBallControls();
            std::ostringstream message;
            message << "asteroid-rgba slot=" << (selectedSlot + 1);
            logCommand(message.str());
            statusText = L"Asteroid RGBA set for object " + std::to_wstring(selectedSlot + 1);
            return true;
        }

        statusText = L"Use: asteroid game | asteroid status/hud/commands/title | asteroid pause/resume/hyperspace | asteroid player set/select | asteroid add/remove | asteroid fire | asteroid score|lives | asteroid alpha/color/window";
        return false;
    }

    if (command == L"add" || command == L"object" || command == L"ball") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Object commands run in bge.game-loop";
            return false;
        }

        size_t slotTokenIndex = 1;
        if (slotTokenIndex < tokens.size()) {
            std::wstring noun = LowerArg(tokens[slotTokenIndex]);
            if (noun == L"object" || noun == L"ball") {
                ++slotTokenIndex;
            }
        }

        int requestedSlot = -1;
        if (slotTokenIndex < tokens.size()) {
            TryParseSlotArg(tokens[slotTokenIndex], requestedSlot);
        }

        int addedSlot = 0;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            AddObjectSlotStateLocked(requestedSlot);
            addedSlot = g_selectedObjectSlot;
        }
        SyncBallControls();
        std::ostringstream message;
        message << "add-object slot=" << (addedSlot + 1);
        logCommand(message.str());
        statusText = L"Added object " + std::to_wstring(addedSlot + 1);
        return true;
    }

    if (command == L"select" || command == L"vector-select") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Select commands run in bge.game-loop";
            return false;
        }

        size_t slotTokenIndex = 1;
        if (slotTokenIndex < tokens.size()) {
            std::wstring noun = LowerArg(tokens[slotTokenIndex]);
            if (noun == L"animation" || noun == L"anim") {
                FocusAnimationFromKeyboard();
                logCommand("select-animation");
                statusText = L"Animation selected";
                return true;
            }
            if (noun == L"object" || noun == L"vector") {
                ++slotTokenIndex;
            }
        }

        if (slotTokenIndex < tokens.size()) {
            std::wstring selectionArg = LowerArg(tokens[slotTokenIndex]);
            if (selectionArg == L"none" || selectionArg == L"clear") {
                {
                    std::lock_guard<std::mutex> lock(ballConfigMutex);
                    ClearObjectSelectionLocked();
                }
                SyncBallControls();
                logCommand("select-none");
                statusText = L"Object selection cleared; Delete commits marked objects";
                return true;
            }
        }

        int slotIndex = 0;
        if (slotTokenIndex >= tokens.size() || !TryParseSlotArg(tokens[slotTokenIndex], slotIndex)) {
            statusText = L"Use: select object 1-10";
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            if (g_objectSlots[slotIndex].isDeleted) {
                statusText = L"Object is deleted; use undo to restore it";
                return false;
            }
            SelectObjectSlotStateLocked(slotIndex);
        }
        SyncBallControls();
        std::ostringstream message;
        message << "select-object slot=" << (slotIndex + 1);
        logCommand(message.str());
        statusText = L"Selected object " + std::to_wstring(slotIndex + 1);
        return true;
    }

    if (command == L"vector" || command == L"velocity") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Vector commands run in bge.game-loop";
            return false;
        }

        size_t valueIndex = 1;
        int slotIndex = -1;
        if (tokens.size() >= 4 && TryParseSlotArg(tokens[valueIndex], slotIndex)) {
            ++valueIndex;
        }
        if (valueIndex + 1 >= tokens.size()) {
            statusText = L"Use: vector [slot] vx vy";
            return false;
        }

        float velocityX = 0.0f;
        float velocityY = 0.0f;
        if (!TryParseFloatArg(tokens[valueIndex], velocityX) || !TryParseFloatArg(tokens[valueIndex + 1], velocityY)) {
            statusText = L"Vector values must be numbers";
            return false;
        }

        int selectedSlot = 0;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            if (slotIndex >= 0) {
                SelectObjectSlotStateLocked(slotIndex);
            }
            ApplyObjectVectorStateLocked(velocityX, velocityY);
            selectedSlot = g_selectedObjectSlot;
        }
        SyncBallControls();
        std::ostringstream message;
        message << "set-vector slot=" << (selectedSlot + 1) << " vx=" << velocityX << " vy=" << velocityY;
        logCommand(message.str());
        statusText = L"Vector set for object " + std::to_wstring(selectedSlot + 1);
        return true;
    }

    if (command == L"color" || command == L"rgb") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Color commands run in bge.game-loop";
            return false;
        }

        size_t valueIndex = 1;
        int slotIndex = -1;
        if (tokens.size() >= 5 && TryParseSlotArg(tokens[valueIndex], slotIndex)) {
            ++valueIndex;
        }
        if (valueIndex + 2 >= tokens.size()) {
            statusText = L"Use: color [slot] r g b";
            return false;
        }

        float colorR = 0.0f;
        float colorG = 0.0f;
        float colorB = 0.0f;
        if (!TryParseFloatArg(tokens[valueIndex], colorR) || !TryParseFloatArg(tokens[valueIndex + 1], colorG) || !TryParseFloatArg(tokens[valueIndex + 2], colorB)) {
            statusText = L"Color values must be numbers";
            return false;
        }

        int selectedSlot = 0;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            if (slotIndex >= 0) {
                SelectObjectSlotStateLocked(slotIndex);
            }
            ApplyObjectColorStateLocked(NormalizeColorArg(colorR), NormalizeColorArg(colorG), NormalizeColorArg(colorB));
            selectedSlot = g_selectedObjectSlot;
        }
        SyncBallControls();
        std::ostringstream message;
        message << "set-color slot=" << (selectedSlot + 1);
        logCommand(message.str());
        statusText = L"Color set for object " + std::to_wstring(selectedSlot + 1);
        return true;
    }

    if (command == L"start" || command == L"run") {
        bool ok = StartAnimationState(statusText);
        logCommand(ok ? "start-animation" : "start-animation failed");
        return ok;
    }

    if (command == L"stop" || command == L"pause") {
        bool ok = StopAnimationState(statusText);
        logCommand(ok ? "stop-animation" : "stop-animation failed");
        return ok;
    }

    if (command == L"animation" || command == L"anim") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Animation commands run in bge.game-loop";
            return false;
        }

        std::wstring subcommand = tokens.size() >= 2 ? LowerArg(tokens[1]) : L"select";
        if (subcommand == L"select" || subcommand == L"focus") {
            FocusAnimationFromKeyboard();
            logCommand("animation-focus");
            statusText = L"Animation selected";
            return true;
        }
        if (subcommand == L"run" || subcommand == L"start" || subcommand == L"play") {
            bool ok = StartAnimationState(statusText);
            logCommand(ok ? "animation-run" : "animation-run failed");
            return ok;
        }
        if (subcommand == L"stop" || subcommand == L"pause") {
            bool ok = StopAnimationState(statusText);
            logCommand(ok ? "animation-stop" : "animation-stop failed");
            return ok;
        }
        if (subcommand == L"step" || subcommand == L"tick") {
            bool ok = StepAnimationOneTick(statusText);
            logCommand(ok ? "animation-step" : "animation-step failed");
            return ok;
        }

        statusText = L"Use: animation select|run|stop|step";
        return false;
    }

    if (command == L"renderer" || command == L"render") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Renderer commands run in bge.game-loop";
            return false;
        }
        bool rendererSet = false;
        if (tokens.size() >= 2) {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            rendererSet = TrySetRendererApiFromArg(tokens[1]);
        }
        if (!rendererSet) {
            statusText = L"Use: renderer dx11|dx12";
            return false;
        }
        if (g_rendererCombo) {
            SendMessageW(g_rendererCombo, CB_SETCURSEL, g_rendererApi.load() == BgeRendererApi::DirectX12 ? 1 : 0, 0);
        }
        logCommand(std::string("switch-renderer renderer=") + RendererApiName());
        statusText = L"Renderer set to " + RendererApiArg();
        return true;
    }

    if (command == L"background" || command == L"bg") {
        if (!CurrentProcessOwnsGameLoop()) {
            statusText = L"Background commands run in bge.game-loop";
            return false;
        }
        if (tokens.size() < 2) {
            statusText = L"Use: background <path>";
            return false;
        }
        std::wstring path = JoinCommandTokens(tokens, 1);
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            g_backgroundImagePath = path;
            g_backgroundImageDirty = true;
        }
        logCommand("load-background");
        statusText = L"Background queued";
        return true;
    }

    if (command == L"sound" || command == L"sound-slot") {
        if (!CurrentProcessOwnsSoundLoop()) {
            statusText = L"Sound commands run in bge.sound";
            return false;
        }
        size_t slotTokenIndex = 1;
        if (slotTokenIndex < tokens.size() && LowerArg(tokens[slotTokenIndex]) == L"slot") {
            ++slotTokenIndex;
        }
        int slotIndex = 0;
        if (slotTokenIndex >= tokens.size() || !TryParseSlotArg(tokens[slotTokenIndex], slotIndex)) {
            statusText = L"Use: sound slot 1-10";
            return false;
        }
        g_activeSoundSlot = slotIndex;
        SyncBallControls();
        std::ostringstream message;
        message << "select-sound-slot slot=" << (slotIndex + 1);
        logCommand(message.str());
        statusText = L"Sound slot " + std::to_wstring(slotIndex + 1);
        return true;
    }

    statusText = L"Unknown command. Try help.";
    logCommand("unknown");
    return false;
}

bool HandleWorkerCommandCopyData(COPYDATASTRUCT* copyData)
{
    if (g_isController || !copyData || copyData->dwData != BGE_COPYDATA_WORKER_COMMAND || !copyData->lpData || copyData->cbData < sizeof(wchar_t)) {
        return false;
    }

    const wchar_t* commandBuffer = static_cast<const wchar_t*>(copyData->lpData);
    std::wstring commandText(commandBuffer);
    std::wstring statusText;
    bool executed = ExecuteCommandText(commandText, statusText);
    if (executed) {
        RecordWorkerCommandHistory(commandText);
        SendWorkerTelemetry(L"command", commandText, statusText);
    }
    else {
        SendWorkerTelemetry(L"command-failed", commandText, statusText);
    }
    SetCommandStatus(L"Controller: " + statusText);

    std::ostringstream message;
    message << "[BgeWorkerCommand] role=" << Narrow(g_workerName) << " command=\"" << Narrow(commandText) << "\" result=" << (executed ? "ok" : "failed");
    LogRendererMessage(message.str());
    return executed;
}

void LoadBackgroundFromDialog()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return;
    }

    wchar_t path[MAX_PATH]{};
    OPENFILENAMEW openFile{};
    openFile.lStructSize = sizeof(openFile);
    openFile.hwndOwner = g_hWnd;
    openFile.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
    openFile.lpstrFile = path;
    openFile.nMaxFile = MAX_PATH;
    openFile.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&openFile)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        g_backgroundImagePath = path;
        g_backgroundImageDirty = true;
    }
}

bool TryStartVectorDrag(int x, int y)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(ballConfigMutex);
    const BgeObjectSlotState& slot = g_objectSlots[g_selectedObjectSlot];
    if (!g_objectSelectionActive || !slot.visible || slot.isDeleted || y < 140) {
        return false;
    }

    float tipX = slot.x + slot.velocityX * 0.35f;
    float tipY = slot.y + slot.velocityY * 0.35f;
    float dx = static_cast<float>(x) - tipX;
    float dy = static_cast<float>(y) - tipY;
    if (dx * dx + dy * dy > 24.0f * 24.0f) {
        return false;
    }

    g_draggingVectorTip = true;
    return true;
}

bool TrySelectObjectAtPoint(int x, int y)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    int selectedSlot = -1;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (y < static_cast<int>(BgeRenderTopInset())) {
            return false;
        }

        for (int index = BGE_OBJECT_SLOT_COUNT - 1; index >= 0; --index) {
            const BgeObjectSlotState& slot = g_objectSlots[index];
            if (!slot.visible || slot.isDeleted) {
                continue;
            }
            float deltaX = static_cast<float>(x) - slot.x;
            float deltaY = static_cast<float>(y) - slot.y;
            if (deltaX * deltaX + deltaY * deltaY <= slot.radius * slot.radius) {
                SelectObjectSlotStateLocked(index);
                selectedSlot = index;
                break;
            }
        }
    }

    if (selectedSlot < 0) {
        return false;
    }

    SyncBallControls();
    SetCommandStatus(L"Selected object " + std::to_wstring(selectedSlot + 1));
    std::ostringstream message;
    message << "[BouncingBallEdit] mouse-select slot=" << (selectedSlot + 1) << " x=" << x << " y=" << y;
    LogRendererMessage(message.str());
    return true;
}

void UpdateVectorDrag(int x, int y)
{
    if (!g_draggingVectorTip) {
        return;
    }

    float velocityX = 0.0f;
    float velocityY = 0.0f;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        BgeObjectSlotState& slot = g_objectSlots[g_selectedObjectSlot];
        velocityX = (static_cast<float>(x) - slot.x) / 0.35f;
        velocityY = (static_cast<float>(y) - slot.y) / 0.35f;
        slot.velocityX = velocityX;
        slot.velocityY = velocityY;
        g_ballVelocityX = velocityX;
        g_ballVelocityY = velocityY;
        g_rendererStateDirty = true;
        PersistActiveObjectGroupLocked();
    }

    wchar_t value[32]{};
    swprintf_s(value, L"%.0f", velocityX);
    if (g_velocityXEdit) SetWindowTextW(g_velocityXEdit, value);
    swprintf_s(value, L"%.0f", velocityY);
    if (g_velocityYEdit) SetWindowTextW(g_velocityYEdit, value);
}

void EndVectorDrag()
{
    if (!g_draggingVectorTip) {
        return;
    }

    g_draggingVectorTip = false;
    std::ostringstream message;
    message << "[BouncingBallControls] drag-vector slot=" << (g_selectedObjectSlot + 1) << " vx=" << g_ballVelocityX << " vy=" << g_ballVelocityY;
    LogRendererMessage(message.str());
}

int ObjectSlotIndexFromNumberKey(WPARAM key)
{
    if (key >= L'1' && key <= L'9') {
        return static_cast<int>(key - L'1');
    }
    if (key == L'0') {
        return 9;
    }
    if (key >= VK_NUMPAD1 && key <= VK_NUMPAD9) {
        return static_cast<int>(key - VK_NUMPAD1);
    }
    if (key == VK_NUMPAD0) {
        return 9;
    }
    return -1;
}

bool SelectObjectSlotFromKeyboard(int slotIndex)
{
    if (!CurrentProcessOwnsGameLoop() || slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
        return false;
    }

    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_objectSlots[slotIndex].isDeleted) {
            statusText = L"Object is deleted; use Ctrl+Z to restore it";
        }
        else {
            SelectObjectSlotStateLocked(slotIndex);
            statusText = L"Selected object " + std::to_wstring(slotIndex + 1);
        }
    }

    SyncBallControls();
    SetCommandStatus(statusText);
    std::ostringstream message;
    message << "[BouncingBallEdit] keyboard-select slot=" << (slotIndex + 1);
    LogRendererMessage(message.str());
    return true;
}

bool FocusObjectGroupsFromKeyboard()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        PersistActiveObjectGroupLocked();
        g_keyboardFocus = BgeKeyboardFocus::Group;
        statusText = L"Group focus: " + ObjectGroupLabel(g_activeObjectGroupIndex);
    }

    SyncBallControls();
    if (g_objectGroupCombo) {
        SetFocus(g_objectGroupCombo);
    }
    SetCommandStatus(statusText);
    LogRendererMessage("[BgeObjectGroup] keyboard-focus");
    return true;
}

bool CycleObjectGroupFromKeyboard(int direction)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    int groupIndex = 0;
    std::wstring groupLabel;
    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_ballAnimationRunning) {
            statusText = L"Stop animation before switching groups";
        }
        else if (g_objectGroups.empty()) {
            statusText = L"No groups";
        }
        else {
            PersistActiveObjectGroupLocked();
            int groupCount = static_cast<int>(g_objectGroups.size());
            groupIndex = (g_activeObjectGroupIndex + direction + groupCount) % groupCount;
            SelectObjectGroupStateLocked(groupIndex);
            g_keyboardFocus = BgeKeyboardFocus::Group;
            groupLabel = ObjectGroupLabel(groupIndex);
            statusText = L"Group: " + groupLabel;
        }
    }

    SyncBallControls();
    if (g_objectGroupCombo) {
        SetFocus(g_objectGroupCombo);
    }
    SetCommandStatus(statusText);
    if (!groupLabel.empty()) {
        std::ostringstream message;
        message << "[BgeObjectGroup] keyboard-select group=" << Narrow(groupLabel) << " index=" << (groupIndex + 1);
        LogRendererMessage(message.str());
    }
    return true;
}

bool StartAnimationState(std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"Start commands run in bge.game-loop";
        return false;
    }

    bool addedObject = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (!AnyObjectSlotVisibleLocked()) {
            AddObjectSlotStateLocked(g_selectedObjectSlot);
            addedObject = true;
        }
        g_keyboardFocus = BgeKeyboardFocus::Animation;
        g_ballAnimationRunning = true;
        g_rendererStateDirty = true;
    }

    SyncBallControls();
    LogRendererMessage(addedObject ? "[BouncingBallControls] start-animation added-object" : "[BouncingBallControls] start-animation");
    statusText = L"Animation started";
    return true;
}

bool StopAnimationState(std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"Stop commands run in bge.game-loop";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        g_keyboardFocus = BgeKeyboardFocus::Animation;
        g_ballAnimationRunning = false;
        g_rendererStateDirty = true;
    }

    SyncBallControls();
    LogRendererMessage("[BouncingBallControls] stop-animation");
    statusText = L"Animation stopped";
    return true;
}

bool StepAnimationOneTick(std::wstring& statusText)
{
    if (!CurrentProcessOwnsGameLoop()) {
        statusText = L"Animation step runs in bge.game-loop";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_ballAnimationRunning) {
            g_keyboardFocus = BgeKeyboardFocus::Animation;
            statusText = L"Animation already running";
            return false;
        }
        if (!AnyObjectSlotVisibleLocked()) {
            g_keyboardFocus = BgeKeyboardFocus::Animation;
            statusText = L"Add an object before stepping animation";
            return false;
        }

        g_keyboardFocus = BgeKeyboardFocus::Animation;
        g_ballAnimationRunning = true;
        g_rendererStateDirty = true;
    }

    {
        std::lock_guard<std::mutex> renderLock(gameLoopMutex);
        ProcessPendingRendererCommands();
        TickActiveRenderer(BGE_ANIMATION_STEP_MILLISECONDS);
        RenderActiveRenderer();
    }

    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        g_ballAnimationRunning = false;
        g_rendererStateDirty = true;
    }

    {
        std::lock_guard<std::mutex> renderLock(gameLoopMutex);
        ProcessPendingRendererCommands();
        RenderActiveRenderer();
    }

    SyncBallControls();
    LogRendererMessage("[BouncingBallControls] step-animation ticks=1");
    statusText = L"Animation stepped one tick";
    return true;
}

bool FocusAnimationFromKeyboard()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        g_keyboardFocus = BgeKeyboardFocus::Animation;
        statusText = g_ballAnimationRunning ? L"Animation focus: running" : L"Animation focus: stopped";
    }

    SetCommandStatus(statusText);
    LogRendererMessage("[BouncingBallControls] keyboard-focus animation");
    return true;
}

bool ConfirmControllerClose(HWND hWnd)
{
    if (!g_isController) {
        return true;
    }

    int result = MessageBoxW(hWnd, L"Close controller?\n\n[Y]es or [N]o", L"Close controller?", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);
    return result == IDYES;
}

bool RendererAnimationRunning()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(ballConfigMutex);
    return g_ballAnimationRunning;
}

void ActivateControllerWindow()
{
    if (g_isController || !g_sharedData || !EnsureCoordMutex()) {
        return;
    }

    DWORD controllerPid = 0;
    {
        CoordLock lock(g_coordMutex);
        if (!lock.locked()) {
            return;
        }
        controllerPid = g_sharedData->controllerPid;
    }

    if (controllerPid == 0 || controllerPid == g_currentPid || !IsProcessAlive(controllerPid)) {
        return;
    }

    HWND controllerWindow = FindWindowForProcess(controllerPid);
    if (!controllerWindow) {
        return;
    }

    ShowWindow(controllerWindow, SW_SHOWNORMAL);
    SetForegroundWindow(controllerWindow);
}

bool CloseWindowFromEscape(HWND sourceWindow)
{
    HWND targetWindow = sourceWindow ? GetAncestor(sourceWindow, GA_ROOT) : g_hWnd;
    if (!targetWindow) {
        targetWindow = g_hWnd;
    }

    SendMessageW(targetWindow, WM_CLOSE, 0, 0);
    if (targetWindow == g_hWnd && !g_isController) {
        ActivateControllerWindow();
    }
    return true;
}

bool HandleEscapeKey(HWND sourceWindow)
{
    HWND targetWindow = sourceWindow ? GetAncestor(sourceWindow, GA_ROOT) : g_hWnd;
    if (!targetWindow) {
        targetWindow = g_hWnd;
    }

    if (targetWindow == g_hWnd && CurrentProcessOwnsGameLoop() && RendererAnimationRunning()) {
        std::wstring statusText;
        ExecuteCommandText(L"stop", statusText);
        SetCommandStatus(statusText.empty() ? L"Renderer stopped" : statusText);
        return true;
    }

    return CloseWindowFromEscape(sourceWindow);
}

bool IsEditControl(HWND sourceWindow)
{
    if (!sourceWindow) {
        return false;
    }

    wchar_t className[32]{};
    GetClassNameW(sourceWindow, className, static_cast<int>(std::size(className)));
    return _wcsicmp(className, L"Edit") == 0;
}

bool HandleEditorShortcutKey(HWND sourceWindow, WPARAM key)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && (key == L'Z' || key == L'z')) {
        std::wstring statusText;
        UndoLastDeleteAction(statusText);
        SetCommandStatus(statusText);
        return true;
    }

    if (key != VK_DELETE || IsEditControl(sourceWindow)) {
        return false;
    }

    std::wstring statusText;
    bool selectionActive = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        selectionActive = g_objectSelectionActive;
    }

    bool handled = selectionActive ? ToggleSelectedObjectDeleteMark(statusText) : CommitMarkedObjectDeletes(statusText);
    SetCommandStatus(statusText);
    return handled || !statusText.empty();
}

bool HandleRendererKeyDown(WPARAM key)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }

    if (key == VK_ESCAPE) {
        HandleEscapeKey(g_hWnd);
        return true;
    }

    int playerIconVisibilityMode = BgePlayerIconVisibilityModeIndexFromKey(static_cast<unsigned int>(key));
    if (playerIconVisibilityMode >= 0) {
        if (HandleAsteroidGameKeyDown(key)) {
            return true;
        }
        if (HandleBgeVectorShipKeyDown(key)) {
            return true;
        }
    }

    if (HandleAsteroidGameKeyDown(key)) {
        return true;
    }

    // Enter starts a new run when the title or game-over screen is showing.
    if (key == VK_RETURN) {
        bool overlayActive = false;
        {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            overlayActive = g_titleScreenActive || g_vectorShipState.gameOver;
        }
        if (overlayActive) {
            std::lock_guard<std::mutex> lock(ballConfigMutex);
            StartOrRestartSpaceRocksGameLocked();
            SendWorkerTelemetry(L"game-event", L"game-start", L"player pressed Enter");
            return true;
        }
    }

    if (HandleBgeVectorShipKeyDown(key)) {
        return true;
    }

    int keyboardSlot = ObjectSlotIndexFromNumberKey(key);
    if (keyboardSlot >= 0) {
        return SelectObjectSlotFromKeyboard(keyboardSlot);
    }

    if (key == VK_OEM_3) {
        return FocusObjectGroupsFromKeyboard();
    }

    bool arrowKey = key == VK_UP || key == VK_DOWN || key == VK_LEFT || key == VK_RIGHT;
    bool animationRunning = false;
    BgeKeyboardFocus keyboardFocus = BgeKeyboardFocus::None;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        animationRunning = g_ballAnimationRunning;
        keyboardFocus = g_keyboardFocus;
    }

    if (keyboardFocus == BgeKeyboardFocus::Animation) {
        std::wstring statusText;
        bool handled = false;
        if (key == L'W') {
            handled = StartAnimationState(statusText);
        }
        else if (key == L'S') {
            handled = StopAnimationState(statusText);
        }
        else if (key == L'D' || key == VK_RIGHT) {
            handled = StepAnimationOneTick(statusText);
        }
        else if (key == L'A' || arrowKey) {
            statusText = L"Animation focus: W run, S stop, D or Right step";
        }

        if (handled || !statusText.empty()) {
            SetCommandStatus(statusText);
            return true;
        }
    }

    if (arrowKey && keyboardFocus == BgeKeyboardFocus::Group) {
        int direction = key == VK_LEFT || key == VK_UP ? -1 : 1;
        return CycleObjectGroupFromKeyboard(direction);
    }

    if (key == VK_UP) {
        CycleEditMode(1);
        return true;
    }
    if (key == VK_DOWN) {
        CycleEditMode(-1);
        return true;
    }
    if (key == VK_LEFT) {
        AdjustEditRate(-1);
        return true;
    }
    if (key == VK_RIGHT) {
        AdjustEditRate(1);
        return true;
    }

    if (animationRunning) {
        SetCommandStatus(L"Stop animation before object editing");
        return true;
    }

    std::wstring statusText;
    bool handled = false;
    float rate = CurrentEditRateMultiplier();
    float translateStep = BGE_TRANSLATE_STEP * rate;
    float resizeStep = BGE_RESIZE_STEP * rate;
    float rotateStep = BGE_ROTATE_STEP_DEGREES * rate;
    float magnitudeStep = BGE_VECTOR_MAGNITUDE_STEP * rate;
    switch (key) {
    case L'W':
        handled = g_editMode == BgeEditMode::Translate ? TranslateSelectedObject(0.0f, -translateStep, statusText)
            : (g_editMode == BgeEditMode::Resize ? ResizeSelectedObject(resizeStep, statusText) : AdjustSelectedObjectVectorMagnitude(magnitudeStep, statusText));
        break;
    case L'A':
        handled = g_editMode == BgeEditMode::Translate ? TranslateSelectedObject(-translateStep, 0.0f, statusText)
            : (g_editMode == BgeEditMode::Resize ? ResizeSelectedObject(-resizeStep, statusText) : RotateSelectedObject(-rotateStep, statusText));
        break;
    case L'S':
        handled = g_editMode == BgeEditMode::Translate ? TranslateSelectedObject(0.0f, translateStep, statusText)
            : (g_editMode == BgeEditMode::Resize ? ResizeSelectedObject(-resizeStep, statusText) : AdjustSelectedObjectVectorMagnitude(-magnitudeStep, statusText));
        break;
    case L'D':
        handled = g_editMode == BgeEditMode::Translate ? TranslateSelectedObject(translateStep, 0.0f, statusText)
            : (g_editMode == BgeEditMode::Resize ? ResizeSelectedObject(resizeStep, statusText) : RotateSelectedObject(rotateStep, statusText));
        break;
    }

    if (handled || !statusText.empty()) {
        SetCommandStatus(statusText);
        return true;
    }
    return false;
}

bool HandleRendererKeyUp(WPARAM key)
{
    if (!CurrentProcessOwnsGameLoop()) {
        return false;
    }
    return HandleBgeVectorShipKeyUp(key);
}

HWND CreateControl(HWND parent, const wchar_t* className, const wchar_t* text, DWORD style, int controlId, int x, int y, int width, int height)
{
    HWND control = CreateWindowW(className, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)), hInst, nullptr);
    if (control) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }
    return control;
}

bool RegisterHistoryWindowClass()
{
    WNDCLASSEXW existing{};
    if (GetClassInfoExW(hInst, kBgeHistoryWindowClass, &existing)) {
        return true;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = HistoryWndProc;
    windowClass.hInstance = hInst;
    windowClass.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BASICGAMEENGINE));
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kBgeHistoryWindowClass;
    windowClass.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

void LayoutControllerHistoryWindow(HWND hWnd)
{
    RECT client{};
    GetClientRect(hWnd, &client);
    int width = (std::max)(320, static_cast<int>(client.right - client.left));
    int height = (std::max)(240, static_cast<int>(client.bottom - client.top));
    int listTop = 34;
    int listHeight = (std::max)(88, (height - 98) / 2);
    int detailLabelTop = listTop + listHeight + 12;
    int detailTop = detailLabelTop + 22;
    int detailHeight = (std::max)(88, height - detailTop - 16);

    if (g_controllerHistoryListLabel) {
        SetWindowPos(g_controllerHistoryListLabel, nullptr, 16, 12, width - 32, 18, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (g_controllerHistoryList) {
        SetWindowPos(g_controllerHistoryList, nullptr, 16, listTop, width - 32, listHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (g_controllerHistoryDetailLabel) {
        SetWindowPos(g_controllerHistoryDetailLabel, nullptr, 16, detailLabelTop, width - 32, 18, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (g_controllerHistoryDetail) {
        SetWindowPos(g_controllerHistoryDetail, nullptr, 16, detailTop, width - 32, detailHeight, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CreateControllerHistoryWindowControls(HWND hWnd)
{
    g_controllerHistoryListLabel = CreateControl(hWnd, L"STATIC", L"Human-readable history", 0, 0, 16, 12, 220, 18);
    g_controllerHistoryList = CreateControl(hWnd, L"LISTBOX", L"", WS_BORDER | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY, IDC_BGE_CONTROLLER_HISTORY, 16, 34, 700, 160);
    g_controllerHistoryDetailLabel = CreateControl(hWnd, L"STATIC", L"Command structure", 0, 0, 16, 210, 220, 18);
    g_controllerHistoryDetail = CreateControl(hWnd, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL, IDC_BGE_CONTROLLER_HISTORY_DETAIL, 16, 232, 700, 150);
    LayoutControllerHistoryWindow(hWnd);
    RefreshControllerHistoryWindow();
    if (!g_controllerHistory.empty()) {
        SendMessageW(g_controllerHistoryList, LB_SETCURSEL, static_cast<WPARAM>(g_controllerHistory.size() - 1), 0);
        UpdateControllerHistoryDetail(g_controllerHistoryDetails.empty() ? L"" : g_controllerHistoryDetails.back());
    }
}

void ShowControllerHistoryWindow()
{
    if (!g_isController) {
        return;
    }

    if (g_controllerHistoryWindow && IsWindow(g_controllerHistoryWindow)) {
        RefreshControllerHistoryWindow();
        ShowWindow(g_controllerHistoryWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_controllerHistoryWindow);
        return;
    }

    if (!RegisterHistoryWindowClass()) {
        SetCommandStatus(L"History window unavailable");
        return;
    }

    g_controllerHistoryWindow = CreateWindowW(kBgeHistoryWindowClass, L"BasicGameEngine - Controller History", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, 760, 460, g_hWnd, nullptr, hInst, nullptr);
    if (!g_controllerHistoryWindow) {
        SetCommandStatus(L"History window unavailable");
        return;
    }

    ShowWindow(g_controllerHistoryWindow, SW_SHOWNORMAL);
    UpdateWindow(g_controllerHistoryWindow);
    SetForegroundWindow(g_controllerHistoryWindow);
    AddControllerHistory(L"Open history window", ControllerBaseCli() + L" --launch-basic-game --ui-command " + QuoteArg(L"history"));
}

LRESULT CALLBACK HistoryWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        CreateControllerHistoryWindowControls(hWnd);
        return 0;

    case WM_SIZE:
        LayoutControllerHistoryWindow(hWnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BGE_CONTROLLER_HISTORY && HIWORD(wParam) == LBN_SELCHANGE) {
            UpdateControllerHistoryDetailFromSelection();
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            SendMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (hWnd == g_controllerHistoryWindow) {
            g_controllerHistoryWindow = nullptr;
            g_controllerHistoryList = nullptr;
            g_controllerHistoryDetail = nullptr;
            g_controllerHistoryListLabel = nullptr;
            g_controllerHistoryDetailLabel = nullptr;
        }
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

bool RegisterMappingWindowClass()
{
    WNDCLASSEXW existing{};
    if (GetClassInfoExW(hInst, kBgeMappingWindowClass, &existing)) {
        return true;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = MappingWndProc;
    windowClass.hInstance = hInst;
    windowClass.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BASICGAMEENGINE));
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kBgeMappingWindowClass;
    windowClass.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

std::wstring MappingWindowText()
{
    std::wstringstream text;
    text << L"BasicGameEngine Renderer Mapping\r\n";
    text << L"Current mode: " << EditModeName(g_editMode) << L"\r\n";
    text << L"Current rate: " << EditRateLabel() << L"\r\n\r\n";
    text << L"Stop / pause\r\n";
    text << L"  Key: Escape while animation is running\r\n";
    text << L"  Animation focus key: S\r\n";
    text << L"  Button: Stop\r\n";
    text << L"  Worker command: stop | animation stop\r\n";
    text << L"  Controller command: game-loop: stop | game-loop: animation stop\r\n\r\n";
    text << L"Window close\r\n";
    text << L"  Keys: Escape when already stopped, or Alt+F4\r\n";
    text << L"  Worker windows: close automatically\r\n";
    text << L"  Closed renderer returns focus to the controller\r\n";
    text << L"  Controller: asks Close controller? [Y]es or [N]o\r\n\r\n";
    text << L"Select object\r\n";
    text << L"  Mouse: click a visible object\r\n";
    text << L"  Keys: 1-9 select objects 1-9; 0 selects object 10\r\n";
    text << L"  Buttons: object slots 1-10\r\n";
    text << L"  Worker command: select <1-10>\r\n";
    text << L"  Controller command: game-loop: select <1-10>\r\n";
    text << L"  Object focus: Up / Down choose edit mode; Left / Right choose rate\r\n";
    text << L"  ASDW edits the selected object according to that mode and rate\r\n\r\n";
    text << L"Delete marking and undo\r\n";
    text << L"  Key: Delete on selected object marks or unmarks it with a red ring\r\n";
    text << L"  Mouse: click empty renderer space to clear object selection\r\n";
    text << L"  Key: Delete with no object selected commits all red-marked objects\r\n";
    text << L"  Key: Ctrl+Z restores the last delete mark, unmark, or commit\r\n";
    text << L"  Worker command: delete [mark|commit] | undo\r\n";
    text << L"  Controller command: game-loop: delete | game-loop: undo\r\n\r\n";
    text << L"Visual state colors\r\n";
    text << L"  Green ring: selected object\r\n";
    text << L"  Red ring: delete-marked object\r\n";
    text << L"  Yellow ring: object collision detected\r\n";
    text << L"  Collision state is logged with scene state for history/replay workflows\r\n\r\n";
    text << L"Object groups\r\n";
    text << L"  Dropdown: choose the active 10-object group\r\n";
    text << L"  + button: add a new named group and switch to its next 10 slots\r\n";
    text << L"  Key: ~ focuses groups; arrows select previous or next group\r\n";
    text << L"  Selecting an object by mouse, key, button, or command returns to object focus\r\n";
    text << L"  Worker command: group add [name] | group select <name|number> | group list\r\n";
    text << L"  Controller command: game-loop: group <subcommand>\r\n\r\n";
    text << L"Ghost group overlay\r\n";
    text << L"  Dropdown: choose a non-active group as read-only ghost context\r\n";
    text << L"  Button: Ghost On / Ghost Off\r\n";
    text << L"  Worker command: ghost group <name|number> | ghost on | ghost off\r\n";
    text << L"  Controller command: game-loop: ghost <subcommand>\r\n\r\n";
    text << L"Main player\r\n";
    text << L"  Button: Set Player marks the selected visible object in the active group\r\n";
    text << L"  Worker command: player set [group] <1-10> | player clear | player status\r\n";
    text << L"  Controller command: game-loop: player <subcommand>\r\n\r\n";
    text << L"Asteroid Game mode\r\n";
    text << L"  Controller button: Asteroid Game\r\n";
    text << L"  Worker command: asteroid game | asteroid-game | game\r\n";
    text << L"  Controller command: asteroid-game | game-loop: asteroid game\r\n";
    text << L"  Component commands: asteroid status | asteroid player set/select | asteroid add/remove | asteroid fire | score | lives | restart\r\n";
    text << L"  Arcade commands: asteroid pause | asteroid resume | asteroid hyperspace\r\n";
    text << L"  Presentation commands: asteroid title | asteroid hud | asteroid commands\r\n";
    text << L"  Rules: asteroid size controls score; ship collisions cost one life; zero lives or cleared asteroids stops the loop\r\n";
    text << L"  When object 1 is selected: A/D or Left/Right rotate, W/Up thrust, S/Down reverse, Space fires, H hyperspace, P pauses/resumes\r\n";
    text << L"  Bullets split large asteroids into smaller asteroids; edge policy switches to wrap\r\n\r\n";
    text << L"Plugin registry\r\n";
    text << L"  Worker command: plugin list | plugin import <plugin-id> | plugin import-set asteroids | plugin explain <plugin-id> | plugin commands\r\n";
    text << L"  Worker command: inspect commands\r\n";
    text << L"  Controller command: game-loop: plugin import bge.piece.vector-ship\r\n";
    text << L"  Vector ship piece: plugin import bge.piece.vector-ship, then player-ship create --id player_ship --shape vector-ship --lives lives --input-profile arrows-space --fire shot --hyperspace H\r\n";
    text << L"  Projectile piece: plugin import bge.piece.projectile, then projectile create --id shot --owner player --shape line --speed 620 --ttl 1.1 --wrap\r\n";
    text << L"  UFO piece: plugin import bge.piece.ufo, then ufo create --id saucer --score score:200 --weapon enemy-shot --arrival 7..18 --edge-spawn\r\n";
    text << L"  Counter capability: counter define score 0 min 0 | counter set score 100 | counter add score 20\r\n";
    text << L"  Scoreboard piece: plugin import bge.piece.scoreboard, then scoreboard create --counters score|high-score|lives|wave --anchor top-left --format SCORE:{score}|HIGH:{high-score}|LIVES:{lives}|WAVE:{wave}\r\n";
    text << L"  Title screen piece: plugin import bge.piece.title-screen, then title-screen create --text ASTEROIDS --subtitle PRESS_ENTER --start Enter --next playing --blink\r\n";
    text << L"  Current phase executes title-screen, scoreboard, vector-ship, projectile, and breakable-rock-field pieces and discovers the other piece signatures\r\n\r\n";
    text << L"Edge policy\r\n";
    text << L"  Worker command: edge bounce | edge wrap | edge clamp | edge status\r\n";
    text << L"  Controller command: game-loop: edge <bounce|wrap|clamp|status>\r\n";
    text << L"  bounce reflects velocity at the border (default)\r\n";
    text << L"  wrap teleports the object across to the opposite border\r\n";
    text << L"  clamp pins the object at the border without reflecting\r\n\r\n";
    text << L"Asteroid alpha sprite\r\n";
    text << L"  Worker command: asteroid add [slot] | asteroid color [slot] r g b [a] | asteroid alpha 0-255\r\n";
    text << L"  Worker command: asteroid window | asteroid status | asteroid ball\r\n";
    text << L"  Controller command: game-loop: asteroid <subcommand>\r\n";
    text << L"  Asteroids use an embedded irregular alpha mesh with no rectangular background\r\n\r\n";
    text << L"Animation focus\r\n";
    text << L"  Buttons: Start or Stop select animation focus\r\n";
    text << L"  Worker command: select animation | animation select|run|stop|step\r\n";
    text << L"  Controller command: game-loop: animation <subcommand>\r\n";
    text << L"  Keys while focused: W run, S stop, D or Right advances one fixed tick while stopped\r\n";
    text << L"  Keyframes can reuse object vector direction and magnitude edits when added\r\n\r\n";
    text << L"Mode selection\r\n";
    text << L"  Keys: Up / Down cycle Translate, Resize, Rotate for object focus or no focus\r\n";
    text << L"  Worker command: mode translate | mode resize | mode rotate\r\n";
    text << L"  Controller command: game-loop: mode <mode>\r\n\r\n";
    text << L"Rate selection\r\n";
    text << L"  Keys: Left slower, Right faster for object focus or no focus\r\n";
    text << L"  Worker command: rate 0.25x | 0.5x | 1x | 2x | 4x | up | down\r\n";
    text << L"  Controller command: game-loop: rate <rate>\r\n\r\n";
    text << L"Translate mode\r\n";
    text << L"  WASD: W up, A left, S down, D right\r\n";
    text << L"  Rate scales the WASD movement step\r\n";
    text << L"  Worker command: translate <dx> <dy>\r\n";
    text << L"  Controller command: game-loop: translate <dx> <dy>\r\n\r\n";
    text << L"Resize mode\r\n";
    text << L"  Positive: D or W\r\n";
    text << L"  Negative: A or S\r\n";
    text << L"  Rate scales the radius delta\r\n";
    text << L"  Worker command: resize <delta-radius>\r\n";
    text << L"  Controller command: game-loop: resize <delta-radius>\r\n\r\n";
    text << L"Rotate mode\r\n";
    text << L"  Direction: A left, D right\r\n";
    text << L"  Magnitude: W stronger, S weaker\r\n";
    text << L"  Rate scales both the degrees delta and magnitude delta\r\n";
    text << L"  Worker command: rotate <degrees>\r\n";
    text << L"  Controller command: game-loop: rotate <degrees>\r\n\r\n";
    text << L"Mapping window\r\n";
    text << L"  Button: Mapping\r\n";
    text << L"  Worker command: mapping\r\n";
    text << L"  Controller command: game-loop: mapping\r\n";
    return text.str();
}

void RefreshMappingWindow()
{
    if (g_mappingText) {
        std::wstring text = MappingWindowText();
        SetWindowTextW(g_mappingText, text.c_str());
    }
}

void LayoutMappingWindow(HWND hWnd)
{
    RECT client{};
    GetClientRect(hWnd, &client);
    int width = (std::max)(360, static_cast<int>(client.right - client.left));
    int height = (std::max)(280, static_cast<int>(client.bottom - client.top));
    if (g_mappingText) {
        SetWindowPos(g_mappingText, nullptr, 16, 16, width - 32, height - 32, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CreateMappingWindowControls(HWND hWnd)
{
    g_mappingText = CreateControl(hWnd, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL, IDC_BGE_MAPPING_TEXT, 16, 16, 600, 420);
    RefreshMappingWindow();
    LayoutMappingWindow(hWnd);
}

void ShowMappingWindow()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return;
    }

    if (g_mappingWindow && IsWindow(g_mappingWindow)) {
        RefreshMappingWindow();
        ShowWindow(g_mappingWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_mappingWindow);
        return;
    }

    if (!RegisterMappingWindowClass()) {
        SetCommandStatus(L"Mapping window unavailable");
        return;
    }

    g_mappingWindow = CreateWindowW(kBgeMappingWindowClass, L"BasicGameEngine - Renderer Mapping", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, 680, 520, g_hWnd, nullptr, hInst, nullptr);
    if (!g_mappingWindow) {
        SetCommandStatus(L"Mapping window unavailable");
        return;
    }

    ShowWindow(g_mappingWindow, SW_SHOWNORMAL);
    UpdateWindow(g_mappingWindow);
    SetForegroundWindow(g_mappingWindow);
    SetCommandStatus(L"Mapping window open");
}

LRESULT CALLBACK MappingWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        CreateMappingWindowControls(hWnd);
        return 0;

    case WM_SIZE:
        LayoutMappingWindow(hWnd);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            SendMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (hWnd == g_mappingWindow) {
            g_mappingWindow = nullptr;
            g_mappingText = nullptr;
        }
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

bool RegisterAsteroidAlphaWindowClass()
{
    WNDCLASSEXW existing{};
    if (GetClassInfoExW(hInst, kBgeAsteroidAlphaWindowClass, &existing)) {
        return true;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = AsteroidAlphaWndProc;
    windowClass.hInstance = hInst;
    windowClass.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BASICGAMEENGINE));
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kBgeAsteroidAlphaWindowClass;
    windowClass.hIconSm = LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

std::wstring AsteroidAlphaStatusText()
{
    std::wstringstream text;
    std::lock_guard<std::mutex> lock(ballConfigMutex);
    const BgeObjectSlotState& slot = g_objectSlots[g_selectedObjectSlot];
    text << L"Object " << (g_selectedObjectSlot + 1) << L": " << BgeObjectShapeName(slot.shape)
         << L" RGBA " << static_cast<int>(slot.colorR * 255.0f)
         << L" " << static_cast<int>(slot.colorG * 255.0f)
         << L" " << static_cast<int>(slot.colorB * 255.0f)
         << L" " << static_cast<int>(slot.colorA * 255.0f);
    if (!slot.visible || slot.isDeleted) {
        text << L" (hidden)";
    }
    return text.str();
}

void RefreshAsteroidAlphaWindow()
{
    if (!g_asteroidAlphaWindow || !IsWindow(g_asteroidAlphaWindow)) {
        return;
    }

    BgeObjectSlotState selectedSlot;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        selectedSlot = g_objectSlots[g_selectedObjectSlot];
    }

    wchar_t value[32]{};
    swprintf_s(value, L"%d", static_cast<int>(selectedSlot.colorR * 255.0f));
    if (g_asteroidREdit) SetWindowTextW(g_asteroidREdit, value);
    swprintf_s(value, L"%d", static_cast<int>(selectedSlot.colorG * 255.0f));
    if (g_asteroidGEdit) SetWindowTextW(g_asteroidGEdit, value);
    swprintf_s(value, L"%d", static_cast<int>(selectedSlot.colorB * 255.0f));
    if (g_asteroidBEdit) SetWindowTextW(g_asteroidBEdit, value);
    swprintf_s(value, L"%d", static_cast<int>(selectedSlot.colorA * 255.0f));
    if (g_asteroidAEdit) SetWindowTextW(g_asteroidAEdit, value);
    if (g_asteroidStatus) SetWindowTextW(g_asteroidStatus, AsteroidAlphaStatusText().c_str());
}

void LayoutAsteroidAlphaWindow(HWND hWnd)
{
    RECT client{};
    GetClientRect(hWnd, &client);
    int width = (std::max)(360, static_cast<int>(client.right - client.left));

    if (g_asteroidStatus) SetWindowPos(g_asteroidStatus, nullptr, 16, 16, width - 32, 22, SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_asteroidREdit) SetWindowPos(g_asteroidREdit, nullptr, 52, 58, 54, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_asteroidGEdit) SetWindowPos(g_asteroidGEdit, nullptr, 130, 58, 54, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_asteroidBEdit) SetWindowPos(g_asteroidBEdit, nullptr, 208, 58, 54, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_asteroidAEdit) SetWindowPos(g_asteroidAEdit, nullptr, 286, 58, 54, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_asteroidApplyRgbaButton) SetWindowPos(g_asteroidApplyRgbaButton, nullptr, 52, 102, 104, 28, SWP_NOZORDER | SWP_NOACTIVATE);
    if (g_asteroidApplyShapeButton) SetWindowPos(g_asteroidApplyShapeButton, nullptr, 168, 102, 128, 28, SWP_NOZORDER | SWP_NOACTIVATE);
}

void ApplyAsteroidAlphaWindowValues(bool ensureVisible)
{
    float colorR = ReadColorControl(g_asteroidREdit, g_ballColorR);
    float colorG = ReadColorControl(g_asteroidGEdit, g_ballColorG);
    float colorB = ReadColorControl(g_asteroidBEdit, g_ballColorB);
    float alpha = ReadColorControl(g_asteroidAEdit, g_ballColorA);
    int selectedSlot = 0;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        ApplyObjectAsteroidStateLocked(colorR, colorG, colorB, alpha, ensureVisible);
        selectedSlot = g_selectedObjectSlot;
    }
    SyncBallControls();
    RefreshAsteroidAlphaWindow();
    std::wstringstream status;
    status << L"Asteroid object " << (selectedSlot + 1) << L" RGBA set";
    SetCommandStatus(status.str());
    LogRendererMessage(std::string("[BgeAsteroidAlpha] set slot=") + std::to_string(selectedSlot + 1));
}

void CreateAsteroidAlphaWindowControls(HWND hWnd)
{
    g_asteroidStatus = CreateControl(hWnd, L"STATIC", L"Asteroid RGBA", 0, IDC_BGE_ASTEROID_STATUS, 16, 16, 360, 22);
    CreateControl(hWnd, L"STATIC", L"R", 0, 0, 28, 62, 18, 20);
    g_asteroidREdit = CreateControl(hWnd, L"EDIT", L"190", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_ASTEROID_R, 52, 58, 54, 24);
    CreateControl(hWnd, L"STATIC", L"G", 0, 0, 106, 62, 18, 20);
    g_asteroidGEdit = CreateControl(hWnd, L"EDIT", L"172", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_ASTEROID_G, 130, 58, 54, 24);
    CreateControl(hWnd, L"STATIC", L"B", 0, 0, 184, 62, 18, 20);
    g_asteroidBEdit = CreateControl(hWnd, L"EDIT", L"150", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_ASTEROID_B, 208, 58, 54, 24);
    CreateControl(hWnd, L"STATIC", L"A", 0, 0, 262, 62, 18, 20);
    g_asteroidAEdit = CreateControl(hWnd, L"EDIT", L"224", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_ASTEROID_A, 286, 58, 54, 24);
    g_asteroidApplyRgbaButton = CreateControl(hWnd, L"BUTTON", L"Set RGBA", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_APPLY_RGBA, 52, 102, 104, 28);
    g_asteroidApplyShapeButton = CreateControl(hWnd, L"BUTTON", L"Set Asteroid", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_APPLY_SHAPE, 168, 102, 128, 28);
    RefreshAsteroidAlphaWindow();
    LayoutAsteroidAlphaWindow(hWnd);
}

void ShowAsteroidAlphaWindow()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return;
    }

    if (g_asteroidAlphaWindow && IsWindow(g_asteroidAlphaWindow)) {
        RefreshAsteroidAlphaWindow();
        ShowWindow(g_asteroidAlphaWindow, SW_SHOWNORMAL);
        SetForegroundWindow(g_asteroidAlphaWindow);
        return;
    }

    if (!RegisterAsteroidAlphaWindowClass()) {
        SetCommandStatus(L"Asteroid Alpha window unavailable");
        return;
    }

    g_asteroidAlphaWindow = CreateWindowW(kBgeAsteroidAlphaWindowClass, L"BasicGameEngine - Asteroid Alpha", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, 400, 190, g_hWnd, nullptr, hInst, nullptr);
    if (!g_asteroidAlphaWindow) {
        SetCommandStatus(L"Asteroid Alpha window unavailable");
        return;
    }

    ShowWindow(g_asteroidAlphaWindow, SW_SHOWNORMAL);
    UpdateWindow(g_asteroidAlphaWindow);
    SetForegroundWindow(g_asteroidAlphaWindow);
    SetCommandStatus(L"Asteroid Alpha window open");
}

LRESULT CALLBACK AsteroidAlphaWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        CreateAsteroidAlphaWindowControls(hWnd);
        return 0;

    case WM_SIZE:
        LayoutAsteroidAlphaWindow(hWnd);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BGE_ASTEROID_APPLY_RGBA) {
            ApplyAsteroidAlphaWindowValues(false);
            return 0;
        }
        if (LOWORD(wParam) == IDC_BGE_ASTEROID_APPLY_SHAPE) {
            ApplyAsteroidAlphaWindowValues(true);
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            SendMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        if (hWnd == g_asteroidAlphaWindow) {
            g_asteroidAlphaWindow = nullptr;
            g_asteroidREdit = nullptr;
            g_asteroidGEdit = nullptr;
            g_asteroidBEdit = nullptr;
            g_asteroidAEdit = nullptr;
            g_asteroidApplyRgbaButton = nullptr;
            g_asteroidApplyShapeButton = nullptr;
            g_asteroidStatus = nullptr;
        }
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

LRESULT CALLBACK CommandEditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN && wParam == VK_RETURN) {
        ExecuteCommandBarInput();
        return 0;
    }

    return CallWindowProcW(g_commandEditOriginalProc, hWnd, message, wParam, lParam);
}

void CreateControllerArtifactRow(HWND hWnd, int artifactIndex, int rowY)
{
    const BgeControllerArtifactSpec& spec = kControllerArtifacts[artifactIndex];
    g_controllerTargetButtons[artifactIndex] = CreateControl(hWnd, L"BUTTON", spec.displayName, BS_AUTORADIOBUTTON | WS_TABSTOP, IDC_BGE_CONTROLLER_TARGET_BASE + artifactIndex, 28, rowY, 150, 22);
    g_controllerStatusLabels[artifactIndex] = CreateControl(hWnd, L"STATIC", L"not running", 0, IDC_BGE_CONTROLLER_STATUS_BASE + artifactIndex, 190, rowY + 3, 410, 20);
    g_controllerLaunchButtons[artifactIndex] = CreateControl(hWnd, L"BUTTON", L"Launch", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_CONTROLLER_LAUNCH_BASE + artifactIndex, 612, rowY - 1, 64, 24);
    g_controllerInspectButtons[artifactIndex] = CreateControl(hWnd, L"BUTTON", L"Open", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_CONTROLLER_INSPECT_BASE + artifactIndex, 684, rowY - 1, 66, 24);
    g_controllerHideButtons[artifactIndex] = CreateControl(hWnd, L"BUTTON", L"Hide", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_CONTROLLER_HIDE_BASE + artifactIndex, 758, rowY - 1, 56, 24);
}

void CreateControllerControls(HWND hWnd)
{
    CreateControl(hWnd, L"STATIC", L"Controller Workspace", 0, 0, 16, 12, 180, 22);
    g_controllerRuntimeStatus = CreateControl(hWnd, L"STATIC", L"workers=0 | window policy=hide nonvisual", 0, IDC_BGE_CONTROLLER_RUNTIME_STATUS, 210, 14, 560, 20);

    CreateControl(hWnd, L"BUTTON", L"Commands And Automation", BS_GROUPBOX, 0, 8, 42, 920, 78);
    CreateControl(hWnd, L"STATIC", L"Target", 0, 0, 24, 72, 48, 20);
    g_controllerTargetLabel = CreateControl(hWnd, L"STATIC", L"Scene And Render / Game Loop", 0, IDC_BGE_CONTROLLER_TARGET_LABEL, 78, 72, 190, 20);
    CreateControl(hWnd, L"STATIC", L"Cmd", 0, 0, 278, 72, 30, 20);
    g_commandEdit = CreateControl(hWnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_COMMAND_EDIT, 312, 68, 300, 24);
    if (g_commandEdit) {
        g_commandEditOriginalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_commandEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CommandEditProc)));
    }
    g_runCommandButton = CreateControl(hWnd, L"BUTTON", L"Run", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_RUN_COMMAND, 620, 68, 52, 24);
    g_commandStatus = CreateControl(hWnd, L"STATIC", L"ready", 0, IDC_BGE_COMMAND_STATUS, 684, 72, 118, 20);
    g_openHistoryButton = CreateControl(hWnd, L"BUTTON", L"History", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_OPEN_HISTORY, 812, 68, 94, 24);
    g_loadAsteroidGameButton = CreateControl(hWnd, L"BUTTON", L"Asteroid Game", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_GAME_PRESET, 684, 94, 118, 22);

    CreateControl(hWnd, L"BUTTON", L"Scene And Render", BS_GROUPBOX, 0, 8, 128, 920, 150);
    CreateControllerArtifactRow(hWnd, 0, 154);
    CreateControllerArtifactRow(hWnd, 1, 184);
    CreateControllerArtifactRow(hWnd, 2, 214);
    CreateControllerArtifactRow(hWnd, 3, 244);

    CreateControl(hWnd, L"BUTTON", L"Assets", BS_GROUPBOX, 0, 8, 288, 920, 58);
    CreateControllerArtifactRow(hWnd, 4, 314);

    CreateControl(hWnd, L"BUTTON", L"Audio", BS_GROUPBOX, 0, 8, 356, 920, 58);
    CreateControllerArtifactRow(hWnd, 5, 382);

    if (!g_cliControllerTarget.empty()) {
        int cliTarget = ResolveControllerArtifactIndex(g_cliControllerTarget);
        if (cliTarget >= 0) {
            g_selectedControllerArtifact = cliTarget;
        }
    }
    SelectControllerArtifact(g_selectedControllerArtifact, false);
    AddControllerHistory(L"Controller UI ready", ControllerBaseCli() + L" --launch-basic-game --ui-target " + QuoteArg(L"Scene And Render") + L" --hide-workers");
}

void CreateBallControls(HWND hWnd)
{
    if (g_playerRuntimeMode) {
        return;
    }

    if (g_isController) {
        CreateControllerControls(hWnd);
        return;
    }

    CreateControl(hWnd, L"STATIC", L"Renderer", 0, 0, 8, 8, 54, 20);
    g_rendererCombo = CreateControl(hWnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, IDC_BGE_RENDERER, 66, 5, 112, 120);
    SendMessageW(g_rendererCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"DirectX 11"));
    SendMessageW(g_rendererCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"DirectX 12"));
    SendMessageW(g_rendererCombo, CB_SETCURSEL, g_rendererApi.load() == BgeRendererApi::DirectX12 ? 1 : 0, 0);

    g_addBallButton = CreateControl(hWnd, L"BUTTON", L"Add Ball", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ADD_BALL, 188, 5, 76, 24);
    g_startAnimationButton = CreateControl(hWnd, L"BUTTON", L"Start", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_START_ANIMATION, 270, 5, 58, 24);
    g_stopAnimationButton = CreateControl(hWnd, L"BUTTON", L"Stop", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_STOP_ANIMATION, 334, 5, 58, 24);

    CreateControl(hWnd, L"STATIC", L"VX", 0, 0, 402, 9, 22, 20);
    g_velocityXEdit = CreateControl(hWnd, L"EDIT", L"180", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_VELOCITY_X, 428, 5, 58, 24);
    CreateControl(hWnd, L"STATIC", L"VY", 0, 0, 494, 9, 22, 20);
    g_velocityYEdit = CreateControl(hWnd, L"EDIT", L"135", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_VELOCITY_Y, 520, 5, 58, 24);
    g_applyVectorButton = CreateControl(hWnd, L"BUTTON", L"Set Vector", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_APPLY_VECTOR, 586, 5, 86, 24);
    g_loadBackgroundButton = CreateControl(hWnd, L"BUTTON", L"Background", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_LOAD_BACKGROUND, 680, 5, 96, 24);
    g_openMappingButton = CreateControl(hWnd, L"BUTTON", L"Mapping", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_OPEN_MAPPING, 784, 5, 80, 24);
    g_editModeStatus = CreateControl(hWnd, L"STATIC", L"Translate 1x", 0, IDC_BGE_EDIT_MODE_STATUS, 872, 9, 100, 20);

    CreateControl(hWnd, L"STATIC", L"Group", 0, 0, 8, 42, 44, 20);
    g_objectGroupCombo = CreateControl(hWnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, IDC_BGE_OBJECT_GROUP_COMBO, 58, 38, 132, 180);
    g_addObjectGroupButton = CreateControl(hWnd, L"BUTTON", L"+", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ADD_OBJECT_GROUP, 198, 38, 30, 24);
    CreateControl(hWnd, L"STATIC", L"Ghost", 0, 0, 238, 42, 42, 20);
    g_ghostGroupCombo = CreateControl(hWnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, IDC_BGE_GHOST_GROUP_COMBO, 286, 38, 132, 180);
    g_toggleGhostButton = CreateControl(hWnd, L"BUTTON", L"Ghost", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_TOGGLE_GHOST, 426, 38, 62, 24);
    g_setPlayerButton = CreateControl(hWnd, L"BUTTON", L"Set Player", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_SET_PLAYER, 496, 38, 86, 24);
    g_playerStatus = CreateControl(hWnd, L"STATIC", L"Player: none", 0, IDC_BGE_PLAYER_STATUS, 592, 42, 360, 20);

    CreateControl(hWnd, L"STATIC", L"Color RGB", 0, 0, 8, 74, 64, 20);
    g_colorREdit = CreateControl(hWnd, L"EDIT", L"245", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_COLOR_R, 76, 70, 44, 24);
    g_colorGEdit = CreateControl(hWnd, L"EDIT", L"87", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_COLOR_G, 126, 70, 44, 24);
    g_colorBEdit = CreateControl(hWnd, L"EDIT", L"56", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_COLOR_B, 176, 70, 44, 24);
    g_applyColorButton = CreateControl(hWnd, L"BUTTON", L"Set Color", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_APPLY_COLOR, 228, 70, 82, 24);

    CreateControl(hWnd, L"STATIC", L"Objects", 0, 0, 320, 74, 54, 20);
    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        wchar_t label[8]{};
        swprintf_s(label, L"%d", index + 1);
        g_objectSlotButtons[index] = CreateControl(hWnd, L"BUTTON", label, BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_OBJECT_SLOT_BASE + index, 378 + index * 34, 70, 30, 24);
    }

    CreateControl(hWnd, L"STATIC", L"Sound", 0, 0, 720, 74, 42, 20);
    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        wchar_t label[8]{};
        swprintf_s(label, L"%d", index + 1);
        g_soundSlotButtons[index] = CreateControl(hWnd, L"BUTTON", label, BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_SOUND_SLOT_BASE + index, 766 + index * 20, 70, 18, 24);
    }

    CreateControl(hWnd, L"STATIC", L"Cmd", 0, 0, 8, 104, 30, 20);
    g_commandEdit = CreateControl(hWnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_COMMAND_EDIT, 42, 100, 260, 24);
    if (g_commandEdit) {
        g_commandEditOriginalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_commandEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CommandEditProc)));
    }
    g_runCommandButton = CreateControl(hWnd, L"BUTTON", L"Run", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_RUN_COMMAND, 310, 100, 46, 24);
    g_asteroidGameWorkerButton = CreateControl(hWnd, L"BUTTON", L"Game", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_GAME_WORKER, 364, 100, 56, 24);
    g_asteroidFireButton = CreateControl(hWnd, L"BUTTON", L"Fire", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_FIRE, 426, 100, 48, 24);
    g_asteroidHyperspaceButton = CreateControl(hWnd, L"BUTTON", L"Hyper", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_HYPERSPACE, 480, 100, 62, 24);
    g_asteroidPauseButton = CreateControl(hWnd, L"BUTTON", L"Pause", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_PAUSE, 548, 100, 58, 24);
    g_asteroidResumeButton = CreateControl(hWnd, L"BUTTON", L"Resume", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_RESUME, 612, 100, 70, 24);
    g_asteroidTitleButton = CreateControl(hWnd, L"BUTTON", L"Title", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_TITLE, 688, 100, 50, 24);
    g_asteroidHudButton = CreateControl(hWnd, L"BUTTON", L"HUD", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_HUD, 744, 100, 46, 24);
    g_asteroidCommandsButton = CreateControl(hWnd, L"BUTTON", L"Help", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_ASTEROID_COMMANDS, 796, 100, 52, 24);
    g_commandStatus = CreateControl(hWnd, L"STATIC", L"help", 0, IDC_BGE_COMMAND_STATUS, 856, 104, 116, 20);
    g_gameHudStatus = CreateControl(hWnd, L"STATIC", L"Game HUD: no module active", 0, IDC_BGE_GAME_HUD_STATUS, 8, 126, 960, 14);
    UpdateEditModeStatus();
    LayoutBallControls(hWnd);
}

void LayoutBallControls(HWND hWnd)
{
    if (g_playerRuntimeMode) {
        return;
    }

    if (g_isController) {
        return;
    }

    RECT client{};
    GetClientRect(hWnd, &client);
    int width = (std::max)(BGE_MIN_WORKER_CLIENT_WIDTH, static_cast<int>(client.right - client.left));
    int right = width - 8;
    bool compact = width < 760;
    bool tiny = width < 560;

    if (g_editModeStatus) {
        SetWindowPos(g_editModeStatus, nullptr, 872, 9, (std::max)(80, right - 872), 20, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    int y = 100;
    int gap = 6;
    int statusWidth = (std::min)(260, (std::max)(116, width / 5));
    int x = right;
    auto placeRight = [&](HWND control, int controlWidth, bool visible = true) {
        if (!control) {
            return;
        }
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
        if (!visible) {
            return;
        }
        x -= controlWidth;
        SetWindowPos(control, nullptr, x, y, controlWidth, 24, SWP_NOZORDER | SWP_NOACTIVATE);
        x -= gap;
    };

    placeRight(g_commandStatus, tiny ? 96 : statusWidth);
    placeRight(g_asteroidCommandsButton, 52, !compact);
    placeRight(g_asteroidHudButton, 46, !compact);
    placeRight(g_asteroidTitleButton, 50, !compact);
    placeRight(g_asteroidResumeButton, 70, !compact);
    placeRight(g_asteroidPauseButton, 58, !tiny);
    placeRight(g_asteroidHyperspaceButton, 62, !tiny);
    placeRight(g_asteroidFireButton, 48);
    placeRight(g_asteroidGameWorkerButton, 56);
    placeRight(g_runCommandButton, 46);

    if (g_commandEdit) {
        int editLeft = 42;
        int editWidth = (std::max)(96, x - editLeft);
        SetWindowPos(g_commandEdit, nullptr, editLeft, y, editWidth, 24, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (g_gameHudStatus) {
        SetWindowPos(g_gameHudStatus, nullptr, 8, 126, (std::max)(96, width - 16), 14, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void UpdateControllerTargetLabel()
{
    if (!g_controllerTargetLabel || g_selectedControllerArtifact < 0 || g_selectedControllerArtifact >= BGE_CONTROLLER_ARTIFACT_COUNT) {
        return;
    }

    const BgeControllerArtifactSpec& spec = kControllerArtifacts[g_selectedControllerArtifact];
    std::wstring label = std::wstring(spec.group) + L" / " + spec.displayName;
    SetWindowTextW(g_controllerTargetLabel, label.c_str());
}

void SelectControllerArtifact(int artifactIndex, bool recordHistory)
{
    if (artifactIndex < 0 || artifactIndex >= BGE_CONTROLLER_ARTIFACT_COUNT) {
        return;
    }

    g_selectedControllerArtifact = artifactIndex;
    for (int index = 0; index < BGE_CONTROLLER_ARTIFACT_COUNT; ++index) {
        if (g_controllerTargetButtons[index]) {
            SendMessageW(g_controllerTargetButtons[index], BM_SETCHECK, index == artifactIndex ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }
    UpdateControllerTargetLabel();
    if (recordHistory) {
        const BgeControllerArtifactSpec& spec = kControllerArtifacts[artifactIndex];
        AddControllerHistory(std::wstring(L"Target -> ") + spec.group + L" / " + spec.displayName, TargetCommandDetail(spec));
    }
}

void LaunchControllerArtifact(int artifactIndex)
{
    if (artifactIndex < 0 || artifactIndex >= BGE_CONTROLLER_ARTIFACT_COUNT) {
        return;
    }
    SelectControllerArtifact(artifactIndex);
    LaunchWorkerRole(kControllerArtifacts[artifactIndex].role);
    SetCommandStatus(std::wstring(L"Launch requested for ") + kControllerArtifacts[artifactIndex].displayName);
    SyncControllerControls();
}

void InspectControllerArtifact(int artifactIndex)
{
    if (artifactIndex < 0 || artifactIndex >= BGE_CONTROLLER_ARTIFACT_COUNT) {
        return;
    }
    SelectControllerArtifact(artifactIndex);
    std::wstring statusText;
    InspectWorkerWindowByRole(kControllerArtifacts[artifactIndex].role, statusText);
    SetCommandStatus(statusText);
    SyncControllerControls();
}

void HideControllerArtifact(int artifactIndex)
{
    if (artifactIndex < 0 || artifactIndex >= BGE_CONTROLLER_ARTIFACT_COUNT) {
        return;
    }
    SelectControllerArtifact(artifactIndex);
    std::wstring statusText;
    HideWorkerWindowByRole(kControllerArtifacts[artifactIndex].role, statusText);
    SetCommandStatus(statusText);
    SyncControllerControls();
}

void LoadAsteroidGameFromController()
{
    if (!g_isController) {
        return;
    }

    int gameLoopIndex = ArtifactIndexForRole(L"bge.game-loop");
    if (gameLoopIndex >= 0) {
        SelectControllerArtifact(gameLoopIndex);
    }

    // Prefer the composed Asteroids recipe (title screen, scoreboard,
    // vector outlines, audio piece) over the legacy `asteroid game`
    // hard-coded mode. This is the same recipe shipped in
    // exports/space-rocks/space-rocks.commands and authored by the
    // command-composed-asteroids contract under kicad_a usability.
    std::wstring recipePath;
    wchar_t exePathBuf[MAX_PATH]{};
    if (GetModuleFileNameW(NULL, exePathBuf, MAX_PATH) > 0) {
        std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();
        const wchar_t* candidates[] = {
            L"exports\\space-rocks\\space-rocks.commands",
            L"..\\..\\exports\\space-rocks\\space-rocks.commands",
            L"..\\..\\..\\exports\\space-rocks\\space-rocks.commands",
        };
        for (const wchar_t* rel : candidates) {
            std::filesystem::path candidate = exeDir / rel;
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec)) {
                recipePath = std::filesystem::weakly_canonical(candidate, ec).wstring();
                if (recipePath.empty()) recipePath = candidate.wstring();
                break;
            }
        }
    }

    if (!recipePath.empty()) {
        std::wstring statusText;
        if (QueueConstructionArtifactCommandsFromFile(recipePath, statusText)) {
            SetCommandStatus(statusText);
            AddControllerHistory(L"Load preset -> Asteroid Game (composed recipe)",
                ControllerBaseCli() + L" --launch-basic-game --game-file " + QuoteArg(recipePath));
            SyncControllerControls();
            return;
        }
        // Fall through to legacy on failure (and surface the reason).
        SetCommandStatus(statusText);
    }

    WorkerSnapshot snapshot = GetWorkerSnapshot(L"bge.game-loop");
    if (!snapshot.running) {
        LaunchWorkerRole(L"bge.game-loop");
    }

    g_pendingControllerCommands.push_back(L"game-loop: asteroid game");

    SetCommandStatus(L"Asteroid Game queued (legacy fallback; recipe not found)");
    AddControllerHistory(L"Load preset -> Asteroid Game (legacy)",
        ControllerBaseCli() + L" --launch-basic-game --ui-command " + QuoteArg(L"asteroid-game"));
    SyncControllerControls();
}

void SyncControllerControls()
{
    if (!g_isController) {
        return;
    }

    LONG workerCount = 0;
    if (g_sharedData && EnsureCoordMutex()) {
        CoordLock lock(g_coordMutex);
        if (lock.locked()) {
            PruneStaleWorkersLocked(g_sharedData);
            workerCount = g_sharedData->workerCount;
        }
    }

    if (g_controllerRuntimeStatus) {
        std::wstring policy = g_hideWorkerWindowsByDefault ? L"hide nonvisual" : L"show workers";
        std::wstring status = L"workers=" + std::to_wstring(workerCount) + L" | window policy=" + policy;
        SetWindowTextW(g_controllerRuntimeStatus, status.c_str());
    }

    for (int artifactIndex = 0; artifactIndex < BGE_CONTROLLER_ARTIFACT_COUNT; ++artifactIndex) {
        const BgeControllerArtifactSpec& spec = kControllerArtifacts[artifactIndex];
        WorkerSnapshot snapshot = GetWorkerSnapshot(spec.role);
        std::wstring statusText;
        if (snapshot.running) {
            statusText = L"running pid=" + std::to_wstring(snapshot.pid) + L" | hb=" + std::to_wstring(snapshot.heartbeatAgeMs / 1000) + L"s | " + WindowVisibilityText(snapshot) + L" | " + spec.summary;
        }
        else {
            statusText = std::wstring(L"not running | ") + spec.summary;
        }

        if (g_controllerStatusLabels[artifactIndex]) {
            SetWindowTextW(g_controllerStatusLabels[artifactIndex], statusText.c_str());
        }
        if (g_controllerLaunchButtons[artifactIndex]) {
            EnableWindow(g_controllerLaunchButtons[artifactIndex], !snapshot.running);
        }
        if (g_controllerInspectButtons[artifactIndex]) {
            EnableWindow(g_controllerInspectButtons[artifactIndex], snapshot.running && snapshot.window != nullptr);
        }
        if (g_controllerHideButtons[artifactIndex]) {
            EnableWindow(g_controllerHideButtons[artifactIndex], snapshot.running && snapshot.window != nullptr && snapshot.windowVisible);
        }
    }

    SelectControllerArtifact(g_selectedControllerArtifact, false);
    if (g_commandEdit) EnableWindow(g_commandEdit, TRUE);
    if (g_runCommandButton) EnableWindow(g_runCommandButton, TRUE);
}

void ProcessControllerUiAutomation()
{
    if (!g_isController) {
        return;
    }

    // Re-entrancy guard: ExecuteControllerCommandText can pump the
    // message loop (IPC), which re-enters WM_TIMER and would invalidate
    // the iterators we are walking over g_pendingControllerCommands /
    // g_cliConstructionArtifactPaths / etc. Without this guard the
    // inner erase() trips _STL_VERIFY in debug builds.
    static bool s_processing = false;
    if (s_processing) {
        return;
    }
    s_processing = true;
    struct Guard { ~Guard() { s_processing = false; } } guard;

    if (!g_cliControllerTarget.empty()) {
        int targetIndex = ResolveControllerArtifactIndex(g_cliControllerTarget);
        if (targetIndex >= 0) {
            SelectControllerArtifact(targetIndex);
        }
        g_cliControllerTarget.clear();
    }

    {
        // Snapshot-and-clear: QueueConstructionArtifactCommandsFromFile
        // may push into g_pendingControllerCommands which can reallocate
        // unrelated vectors during message-loop pumping; also defends
        // against the called code re-entering this list.
        std::vector<std::wstring> pendingArtifactPaths;
        pendingArtifactPaths.swap(g_cliConstructionArtifactPaths);
        for (const auto& artifactPath : pendingArtifactPaths) {
            std::wstring statusText;
            QueueConstructionArtifactCommandsFromFile(artifactPath, statusText);
            SetCommandStatus(statusText);
        }
    }

    for (auto selector = g_cliInspectSelectors.begin(); selector != g_cliInspectSelectors.end();) {
        int targetIndex = ResolveControllerArtifactIndex(*selector);
        if (targetIndex < 0) {
            selector = g_cliInspectSelectors.erase(selector);
            continue;
        }

        std::wstring statusText;
        if (InspectWorkerWindowByRole(kControllerArtifacts[targetIndex].role, statusText)) {
            SetCommandStatus(statusText);
            selector = g_cliInspectSelectors.erase(selector);
        }
        else {
            ++selector;
        }
    }

    for (auto selector = g_cliHideSelectors.begin(); selector != g_cliHideSelectors.end();) {
        int targetIndex = ResolveControllerArtifactIndex(*selector);
        if (targetIndex < 0) {
            selector = g_cliHideSelectors.erase(selector);
            continue;
        }

        std::wstring statusText;
        if (HideWorkerWindowByRole(kControllerArtifacts[targetIndex].role, statusText)) {
            SetCommandStatus(statusText);
            selector = g_cliHideSelectors.erase(selector);
        }
        else {
            ++selector;
        }
    }

    if (g_hideWorkerWindowsByDefault) {
        for (int artifactIndex = 0; artifactIndex < BGE_CONTROLLER_ARTIFACT_COUNT; ++artifactIndex) {
            const BgeControllerArtifactSpec& spec = kControllerArtifacts[artifactIndex];
            if (spec.visualByDefault || RoleVectorContains(g_revealedWorkerRoles, spec.role)) {
                continue;
            }
            WorkerSnapshot snapshot = GetWorkerSnapshot(spec.role);
            if (snapshot.running && snapshot.window && snapshot.windowVisible) {
                std::wstring statusText;
                HideWorkerWindowByRole(spec.role, statusText);
            }
        }
    }

    {
        // Snapshot-and-replace: ExecuteControllerCommandText runs user
        // commands that may push more entries into g_pendingControllerCommands
        // (e.g. the Asteroid Game button enqueues a whole recipe). A
        // push_back can reallocate the vector and invalidate the iterator
        // we are holding. Snapshot the queue, retain only the unexecuted
        // tail (commands whose target worker is not yet running), and let
        // any newly-pushed commands accumulate at the back for next tick.
        std::vector<std::wstring> pending;
        pending.swap(g_pendingControllerCommands);
        std::vector<std::wstring> requeue;
        requeue.reserve(pending.size());
        for (const auto& cmd : pending) {
            std::wstring targetRole = SelectedControllerRole();
            if (targetRole.empty() || !GetWorkerSnapshot(targetRole).running) {
                requeue.push_back(cmd);
                continue;
            }
            std::wstring statusText;
            bool executed = ExecuteControllerCommandText(cmd, statusText);
            SetCommandStatus(statusText);
            if (!executed && statusText.find(L"not running") != std::wstring::npos) {
                requeue.push_back(cmd);
            }
        }
        // Re-prepend the unexecuted items so they retry before any newly
        // pushed commands from this tick.
        if (!requeue.empty()) {
            requeue.insert(requeue.end(),
                           g_pendingControllerCommands.begin(),
                           g_pendingControllerCommands.end());
            g_pendingControllerCommands.swap(requeue);
        }
    }
}

void SyncObjectGroupControls()
{
    if (g_isController) {
        return;
    }

    std::vector<std::wstring> labels;
    int activeGroupIndex = 0;
    int ghostGroupIndex = -1;
    bool ghostEnabled = false;
    int playerGroupIndex = -1;
    int playerSlot = -1;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        PersistActiveObjectGroupLocked();
        activeGroupIndex = g_activeObjectGroupIndex;
        ghostGroupIndex = g_ghostObjectGroupIndex;
        ghostEnabled = g_ghostOverlayEnabled;
        playerGroupIndex = g_mainPlayerGroupIndex;
        playerSlot = g_mainPlayerSlot;
        for (const auto& group : g_objectGroups) {
            std::wstring label = group.label;
            if (group.isDeleted) {
                label = L"x " + label;
            }
            else if (group.deleteMarked) {
                label = L"D " + label;
            }
            labels.push_back(label);
        }
    }

    if (g_objectGroupCombo) {
        SendMessageW(g_objectGroupCombo, CB_RESETCONTENT, 0, 0);
        for (const auto& label : labels) {
            SendMessageW(g_objectGroupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        SendMessageW(g_objectGroupCombo, CB_SETCURSEL, static_cast<WPARAM>((std::max)(0, activeGroupIndex)), 0);
    }

    if (g_ghostGroupCombo) {
        SendMessageW(g_ghostGroupCombo, CB_RESETCONTENT, 0, 0);
        SendMessageW(g_ghostGroupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"<none>"));
        for (const auto& label : labels) {
            SendMessageW(g_ghostGroupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        int ghostSelection = ghostGroupIndex >= 0 ? ghostGroupIndex + 1 : 0;
        SendMessageW(g_ghostGroupCombo, CB_SETCURSEL, static_cast<WPARAM>(ghostSelection), 0);
    }

    if (g_toggleGhostButton) {
        SetWindowTextW(g_toggleGhostButton, ghostEnabled ? L"Ghost On" : L"Ghost Off");
    }

    if (g_playerStatus) {
        std::wstring status = L"Player: none";
        if (playerGroupIndex >= 0 && playerGroupIndex < static_cast<int>(labels.size()) && playerSlot >= 0 && playerSlot < BGE_OBJECT_SLOT_COUNT) {
            status = L"Player: " + labels[static_cast<size_t>(playerGroupIndex)] + L" / object " + std::to_wstring(playerSlot + 1);
        }
        SetWindowTextW(g_playerStatus, status.c_str());
    }
}

void SelectObjectGroupFromControls()
{
    if (!CurrentProcessOwnsGameLoop() || !g_objectGroupCombo) {
        return;
    }

    LRESULT selection = SendMessageW(g_objectGroupCombo, CB_GETCURSEL, 0, 0);
    if (selection == CB_ERR) {
        return;
    }

    std::wstring statusText;
    bool switched = false;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_ballAnimationRunning) {
            statusText = L"Stop animation before switching groups";
        }
        else {
            switched = SelectObjectGroupStateLocked(static_cast<int>(selection));
            statusText = switched ? L"Group: " + ObjectGroupLabel(g_activeObjectGroupIndex) : L"Group not found";
        }
    }

    SyncBallControls();
    if (g_objectGroupCombo) {
        SetFocus(g_objectGroupCombo);
    }
    SetCommandStatus(statusText);
    if (switched) {
        std::ostringstream message;
        message << "[BgeObjectGroup] active group=" << Narrow(ObjectGroupLabel(static_cast<int>(selection))) << " index=" << (static_cast<int>(selection) + 1);
        LogRendererMessage(message.str());
    }
}

void AddObjectGroupFromControls()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return;
    }

    int groupIndex = -1;
    std::wstring groupLabel;
    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_ballAnimationRunning) {
            statusText = L"Stop animation before adding a group";
        }
        else {
            groupIndex = AddObjectGroupStateLocked(L"");
            groupLabel = ObjectGroupLabel(groupIndex);
            statusText = L"Added group: " + groupLabel;
        }
    }

    SyncBallControls();
    if (g_objectGroupCombo) {
        SetFocus(g_objectGroupCombo);
    }
    SetCommandStatus(statusText);
    if (groupIndex >= 0) {
        std::ostringstream message;
        message << "[BgeObjectGroup] add group=" << Narrow(groupLabel) << " index=" << (groupIndex + 1);
        LogRendererMessage(message.str());
    }
}

void SelectGhostGroupFromControls()
{
    if (!CurrentProcessOwnsGameLoop() || !g_ghostGroupCombo) {
        return;
    }

    LRESULT selection = SendMessageW(g_ghostGroupCombo, CB_GETCURSEL, 0, 0);
    if (selection == CB_ERR) {
        return;
    }

    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (selection == 0) {
            g_ghostObjectGroupIndex = -1;
            g_ghostOverlayEnabled = false;
            statusText = L"Ghost group cleared";
        }
        else {
            g_ghostObjectGroupIndex = static_cast<int>(selection) - 1;
            g_ghostOverlayEnabled = g_ghostObjectGroupIndex != g_activeObjectGroupIndex;
            statusText = g_ghostOverlayEnabled ? L"Ghosting: " + ObjectGroupLabel(g_ghostObjectGroupIndex) : L"Ghost group matches active group";
        }
        g_rendererStateDirty = true;
    }

    SyncBallControls();
    SetCommandStatus(statusText);
}

void ToggleGhostGroupFromControls()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return;
    }

    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (g_ghostOverlayEnabled) {
            g_ghostOverlayEnabled = false;
            statusText = L"Ghost off";
        }
        else {
            if (g_ghostObjectGroupIndex < 0 || g_ghostObjectGroupIndex == g_activeObjectGroupIndex) {
                g_ghostObjectGroupIndex = -1;
                for (int groupIndex = 0; groupIndex < static_cast<int>(g_objectGroups.size()); ++groupIndex) {
                    if (groupIndex != g_activeObjectGroupIndex) {
                        g_ghostObjectGroupIndex = groupIndex;
                        break;
                    }
                }
            }
            if (g_ghostObjectGroupIndex < 0) {
                statusText = L"Add another group before ghosting";
            }
            else {
                g_ghostOverlayEnabled = true;
                statusText = L"Ghosting: " + ObjectGroupLabel(g_ghostObjectGroupIndex);
            }
        }
        g_rendererStateDirty = true;
    }

    SyncBallControls();
    SetCommandStatus(statusText);
}

void SetMainPlayerFromControls()
{
    if (!CurrentProcessOwnsGameLoop()) {
        return;
    }

    std::wstring statusText;
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        if (!g_objectSelectionActive || !g_objectSlots[g_selectedObjectSlot].visible || g_objectSlots[g_selectedObjectSlot].isDeleted) {
            statusText = L"Add or select a visible object first";
        }
        else {
            g_mainPlayerGroupIndex = g_activeObjectGroupIndex;
            g_mainPlayerSlot = g_selectedObjectSlot;
            PersistActiveObjectGroupLocked();
            statusText = L"Main player: " + ObjectGroupLabel(g_mainPlayerGroupIndex) + L" / object " + std::to_wstring(g_mainPlayerSlot + 1);
        }
    }

    SyncBallControls();
    SetCommandStatus(statusText);
}

void ExecuteAsteroidCommandFromControls(const wchar_t* commandText)
{
    if (!CurrentProcessOwnsGameLoop() || !commandText) {
        return;
    }

    std::wstring statusText;
    ExecuteCommandText(commandText, statusText);
    SetCommandStatus(statusText);
    if (g_hWnd) {
        SetFocus(g_hWnd);
    }
}

void SyncBallControls()
{
    if (g_isController) {
        SyncControllerControls();
        return;
    }

    bool enabled = CurrentProcessOwnsGameLoop();
    bool soundEnabled = CurrentProcessOwnsSoundLoop();
    bool commandEnabled = enabled || soundEnabled;
    BgeObjectSlotState selectedSlot;
    int selectedSlotIndex = 0;
    int activeGroupIndex = 0;
    int mainPlayerGroupIndex = -1;
    int mainPlayerSlot = -1;
    bool objectSelectionActive = false;
    std::array<bool, BGE_OBJECT_SLOT_COUNT> visibleSlots{};
    std::array<bool, BGE_OBJECT_SLOT_COUNT> markedSlots{};
    std::array<bool, BGE_OBJECT_SLOT_COUNT> deletedSlots{};
    {
        std::lock_guard<std::mutex> lock(ballConfigMutex);
        activeGroupIndex = g_activeObjectGroupIndex;
        mainPlayerGroupIndex = g_mainPlayerGroupIndex;
        mainPlayerSlot = g_mainPlayerSlot;
        objectSelectionActive = g_objectSelectionActive;
        selectedSlotIndex = g_selectedObjectSlot;
        selectedSlot = g_objectSlots[selectedSlotIndex];
        for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
            visibleSlots[index] = g_objectSlots[index].visible && !g_objectSlots[index].isDeleted;
            markedSlots[index] = g_objectSlots[index].deleteMarked;
            deletedSlots[index] = g_objectSlots[index].isDeleted;
        }
    }

    wchar_t value[32]{};
    swprintf_s(value, L"%.0f", selectedSlot.velocityX);
    if (g_velocityXEdit) SetWindowTextW(g_velocityXEdit, value);
    swprintf_s(value, L"%.0f", selectedSlot.velocityY);
    if (g_velocityYEdit) SetWindowTextW(g_velocityYEdit, value);
    swprintf_s(value, L"%d", static_cast<int>(selectedSlot.colorR * 255.0f));
    if (g_colorREdit) SetWindowTextW(g_colorREdit, value);
    swprintf_s(value, L"%d", static_cast<int>(selectedSlot.colorG * 255.0f));
    if (g_colorGEdit) SetWindowTextW(g_colorGEdit, value);
    swprintf_s(value, L"%d", static_cast<int>(selectedSlot.colorB * 255.0f));
    if (g_colorBEdit) SetWindowTextW(g_colorBEdit, value);
    RefreshAsteroidAlphaWindow();

    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        wchar_t label[16]{};
        bool isMainPlayer = mainPlayerGroupIndex == activeGroupIndex && mainPlayerSlot == index;
        bool isSelected = objectSelectionActive && index == selectedSlotIndex;
        if (deletedSlots[index]) {
            swprintf_s(label, L"x%d", index + 1);
        }
        else if (isSelected && markedSlots[index]) {
            swprintf_s(label, L"[D]");
        }
        else if (markedSlots[index]) {
            swprintf_s(label, L"D%d", index + 1);
        }
        else if (isSelected && isMainPlayer) {
            swprintf_s(label, L"[P]");
        }
        else if (isSelected) {
            swprintf_s(label, L"[%d]", index + 1);
        }
        else if (isMainPlayer) {
            swprintf_s(label, L"P%d", index + 1);
        }
        else if (visibleSlots[index]) {
            swprintf_s(label, L"%d*", index + 1);
        }
        else {
            swprintf_s(label, L"%d", index + 1);
        }
        if (g_objectSlotButtons[index]) {
            SetWindowTextW(g_objectSlotButtons[index], label);
        }
    }

    for (int index = 0; index < BGE_OBJECT_SLOT_COUNT; ++index) {
        wchar_t label[16]{};
        if (index == g_activeSoundSlot) {
            swprintf_s(label, L"[%d]", index + 1);
        }
        else {
            swprintf_s(label, L"%d", index + 1);
        }
        if (g_soundSlotButtons[index]) {
            SetWindowTextW(g_soundSlotButtons[index], label);
        }
    }

    HWND controls[] = {
        g_rendererCombo,
        g_addBallButton,
        g_startAnimationButton,
        g_stopAnimationButton,
        g_loadBackgroundButton,
        g_openMappingButton,
        g_editModeStatus,
        g_velocityXEdit,
        g_velocityYEdit,
        g_applyVectorButton,
        g_colorREdit,
        g_colorGEdit,
        g_colorBEdit,
        g_applyColorButton,
        g_objectGroupCombo,
        g_addObjectGroupButton,
        g_ghostGroupCombo,
        g_toggleGhostButton,
        g_setPlayerButton,
        g_playerStatus,
        g_gameHudStatus,
        g_asteroidGameWorkerButton,
        g_asteroidFireButton,
        g_asteroidHyperspaceButton,
        g_asteroidPauseButton,
        g_asteroidResumeButton,
        g_asteroidTitleButton,
        g_asteroidHudButton,
        g_asteroidCommandsButton,
        g_commandEdit,
        g_runCommandButton,
    };
    for (size_t index = 0; index < std::size(controls); ++index) {
        HWND control = controls[index];
        if (control) EnableWindow(control, enabled);
    }
    if (g_commandEdit) EnableWindow(g_commandEdit, commandEnabled);
    if (g_runCommandButton) EnableWindow(g_runCommandButton, commandEnabled);
    for (HWND control : g_objectSlotButtons) {
        if (control) EnableWindow(control, enabled);
    }
    for (HWND control : g_soundSlotButtons) {
        if (control) EnableWindow(control, soundEnabled);
    }
    SyncObjectGroupControls();
}

bool EnsureCoordMutex()
{
    if (g_coordMutex) {
        return true;
    }

    g_coordMutex = CreateMutexW(NULL, FALSE, g_bgeCoordMutexName.c_str());
    return g_coordMutex != NULL;
}

bool IsProcessAlive(DWORD pid)
{
    if (pid == 0) {
        return false;
    }
    if (pid == g_currentPid) {
        return true;
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return false;
    }

    DWORD exitCode = 0;
    bool alive = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
}

void RecountWorkersLocked(SharedMemoryData* sharedData)
{
    LONG count = 0;
    for (LONG i = 0; i < BGE_MAX_WORKERS; ++i) {
        if (sharedData->workers[i].pid != 0) {
            ++count;
        }
    }
    sharedData->workerCount = count;
}

void RemoveWorkerLocked(SharedMemoryData* sharedData, DWORD pid)
{
    for (LONG i = 0; i < BGE_MAX_WORKERS; ++i) {
        if (sharedData->workers[i].pid == pid) {
            ZeroMemory(&sharedData->workers[i], sizeof(sharedData->workers[i]));
        }
    }
    RecountWorkersLocked(sharedData);
}

void PruneStaleWorkersLocked(SharedMemoryData* sharedData)
{
    for (LONG i = 0; i < BGE_MAX_WORKERS; ++i) {
        DWORD pid = sharedData->workers[i].pid;
        if (pid != 0 && !IsProcessAlive(pid)) {
            ZeroMemory(&sharedData->workers[i], sizeof(sharedData->workers[i]));
        }
    }
    RecountWorkersLocked(sharedData);
}

bool UpsertWorkerLocked(SharedMemoryData* sharedData, DWORD pid, const std::wstring& name)
{
    LONG target = -1;
    for (LONG i = 0; i < BGE_MAX_WORKERS; ++i) {
        if (sharedData->workers[i].pid == pid) {
            target = i;
            break;
        }
        if (target == -1 && sharedData->workers[i].pid == 0) {
            target = i;
        }
    }

    if (target == -1) {
        return false;
    }

    sharedData->workers[target].pid = pid;
    sharedData->workers[target].role = BGE_ROLE_WORKER;
    sharedData->workers[target].heartbeatTick = GetTickCount64();
    wcsncpy_s(sharedData->workers[target].name, _countof(sharedData->workers[target].name), name.c_str(), _TRUNCATE);
    RecountWorkersLocked(sharedData);
    return true;
}

bool RegisterCurrentProcess(SharedMemoryData* sharedData)
{
    if (!sharedData || !EnsureCoordMutex()) {
        return false;
    }

    if (g_workerName.empty() || g_workerName == L"worker") {
        std::wstringstream name;
        name << L"worker-" << g_currentPid;
        g_workerName = name.str();
    }

    CoordLock lock(g_coordMutex);
    if (!lock.locked()) {
        return false;
    }

    PruneStaleWorkersLocked(sharedData);

    if (sharedData->controllerPid == 0 || !IsProcessAlive(sharedData->controllerPid)) {
        if (g_workerRoleRequested) {
            sharedData->controllerPid = 0;
            sharedData->controllerHeartbeatTick = 0;
            g_isController = false;
            return UpsertWorkerLocked(sharedData, g_currentPid, g_workerName);
        }

        sharedData->controllerPid = g_currentPid;
        sharedData->controllerHeartbeatTick = GetTickCount64();
        RemoveWorkerLocked(sharedData, g_currentPid);
        g_isController = true;
        return true;
    }

    g_isController = false;
    return UpsertWorkerLocked(sharedData, g_currentPid, g_workerName);
}

void UnregisterCurrentProcess()
{
    if (!g_sharedData || !EnsureCoordMutex()) {
        return;
    }

    CoordLock lock(g_coordMutex);
    if (!lock.locked()) {
        return;
    }

    if (g_sharedData->controllerPid == g_currentPid) {
        g_sharedData->controllerPid = 0;
        g_sharedData->controllerHeartbeatTick = 0;
    }

    RemoveWorkerLocked(g_sharedData, g_currentPid);
}

void UpdateCurrentHeartbeat()
{
    if (!g_sharedData || !EnsureCoordMutex()) {
        return;
    }

    DWORD64 now = GetTickCount64();
    if (now - g_lastHeartbeatTick < 1000) {
        return;
    }
    g_lastHeartbeatTick = now;

    CoordLock lock(g_coordMutex);
    if (!lock.locked()) {
        return;
    }

    if (g_isController && g_sharedData->controllerPid == g_currentPid) {
        g_sharedData->controllerHeartbeatTick = now;
        PruneStaleWorkersLocked(g_sharedData);
    }
    else if (!g_isController) {
        UpsertWorkerLocked(g_sharedData, g_currentPid, g_workerName);
    }
}

struct CloseWindowContext {
    DWORD pid;
    bool posted;
};

BOOL CALLBACK CloseWindowForProcessCallback(HWND hwnd, LPARAM lParam)
{
    CloseWindowContext* context = reinterpret_cast<CloseWindowContext*>(lParam);
    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);

    if (windowPid == context->pid && IsWindow(hwnd)) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
        context->posted = true;
    }

    return TRUE;
}

void RequestManagedWorkersClose()
{
    if (g_closeBroadcastSent || !g_isController || !g_sharedData || !EnsureCoordMutex()) {
        return;
    }

    g_closeBroadcastSent = true;
    std::vector<DWORD> workerPids;

    {
        CoordLock lock(g_coordMutex);
        if (!lock.locked()) {
            return;
        }

        PruneStaleWorkersLocked(g_sharedData);
        for (LONG i = 0; i < BGE_MAX_WORKERS; ++i) {
            DWORD pid = g_sharedData->workers[i].pid;
            if (pid != 0 && pid != g_currentPid) {
                workerPids.push_back(pid);
            }
        }
    }

    for (DWORD pid : workerPids) {
        CloseWindowContext context{ pid, false };
        EnumWindows(CloseWindowForProcessCallback, reinterpret_cast<LPARAM>(&context));
    }
}

bool CurrentProcessOwnsGameLoop()
{
    return !g_isController && g_workerName == L"bge.game-loop";
}

bool CurrentProcessOwnsSoundLoop()
{
    return !g_isController && g_workerName == L"bge.sound";
}

// Game loop function running in a separate thread
void GameLoop()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER previousTime, currentTime;
    QueryPerformanceCounter(&previousTime);

    const int targetFPS = 60;
    const double targetFrameDuration = 1000.0 / targetFPS;
    const bool ownsGameLoop = CurrentProcessOwnsGameLoop();

    while (shouldRun) {
        UpdateCurrentHeartbeat();

        if (!ownsGameLoop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(gameLoopMutex);
            ProcessPendingRendererCommands();
        }

        if (!DirectXRendererActive() && (!isFocused || GetForegroundWindow() != g_hWnd)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::lock_guard<std::mutex> lock(gameLoopMutex);

        QueryPerformanceCounter(&currentTime);
        double deltaTime = (double)(currentTime.QuadPart - previousTime.QuadPart) * 1000.0 / frequency.QuadPart;
        previousTime = currentTime;

        if (DirectXRendererActive()) {
            TickActiveRenderer(deltaTime);
            RenderActiveRenderer();
        }
        else if (g_showMutexLine) {
            if (movingRight) {
                lineX += speed * deltaTime;
                if (lineX >= 200) movingRight = false;
            }
            else {
                lineX -= speed * deltaTime;
                if (lineX <= 10) movingRight = true;
            }

            RECT updateRect;
            updateRect.left = 0;
            updateRect.top = 0;
            updateRect.right = 220;
            updateRect.bottom = 220;

            InvalidateRect(g_hWnd, &updateRect, FALSE);
        }

        if (!g_playerRuntimeMode) {
            statusBarMgr.Update();
        }

        QueryPerformanceCounter(&currentTime);
        double frameTime = (double)(currentTime.QuadPart - previousTime.QuadPart) * 1000.0 / frequency.QuadPart;

        // Introduce a more precise delay to cap the frame rate
        if (frameTime < targetFrameDuration) {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(targetFrameDuration - frameTime - 1)));
            while (true) {
                QueryPerformanceCounter(&currentTime);
                frameTime = (double)(currentTime.QuadPart - previousTime.QuadPart) * 1000.0 / frequency.QuadPart;
                if (frameTime >= targetFrameDuration)
                    break;
            }
        }
    }
}

//// Update your `StatusBarMgr` class to read the `instanceCount` from shared memory whenever necessary.
//void StatusBarMgr::Update()
//{
//    // Get updated data from shared memory
//    SharedMemoryData* sharedData = reinterpret_cast<SharedMemoryData*>(comm->GetSharedMemoryPointer());
//    if (!sharedData) return;
//
//    std::wstring statusText = statusBar.GetStatus();
//    double fps = statusBar.GetFramerate();
//
//    wchar_t displayText[100];
//    swprintf_s(displayText, 100, L"%s | FPS: %.2f | Instances: %d", statusText.c_str(), fps, sharedData->instanceCount);
//
//    UpdateText(displayText);
//}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BASICGAMEENGINE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = g_playerRuntimeMode ? static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)) : (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = g_playerRuntimeMode ? nullptr : MAKEINTRESOURCEW(IDC_BASICGAMEENGINE);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, 0, 980, 720, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    g_hWnd = hWnd; // Store the handle in a global variable

    if (g_playerRuntimeMode) {
        SetMenu(hWnd, nullptr);
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Initialize the taskbar manager
    taskBarMgr.Initialize(hWnd, hInstance);

    if (!g_playerRuntimeMode) {
        // Create the status bar
        statusBarMgr.Create(hWnd, hInstance);

        // Update the status bar with the privilege status
        std::wstring privilegeStatus = userPrivilegeMgr.GetUserPrivilege().GetPrivilegeStatus();
        statusBarMgr.UpdatePrivilegeStatus(privilegeStatus);
    }

    // Set a timer to update every 1000 milliseconds (1 second)
    SetTimer(hWnd, 1, 1000, NULL);

    return TRUE;
}

// Window Procedure to handle window events
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CLOSE:
        if (g_isController && !ConfirmControllerClose(hWnd)) {
            break;
        }
        if (g_isController) {
            RequestManagedWorkersClose();
        }
        DestroyWindow(hWnd);
        break;

    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (TrySelectObjectAtPoint(x, y)) {
            break;
        }
        if (TryStartVectorDrag(x, y)) {
            SetCapture(hWnd);
            break;
        }
        if (ClearObjectSelectionFromRendererClick(x, y)) {
            break;
        }

        // Update instance count when the window is clicked
        if (isFocused)
        {
            int instanceCount = g_sharedData ? g_sharedData->instanceCount : 0; // Get the updated count from shared memory
            statusBarMgr.UpdateInstanceCount(instanceCount); // Update the status bar with the new count
        }
    }
    break;

    case WM_MOUSEMOVE:
        if (g_draggingVectorTip) {
            UpdateVectorDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        break;

    case WM_LBUTTONUP:
        if (g_draggingVectorTip) {
            UpdateVectorDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            EndVectorDrag();
            ReleaseCapture();
        }
        break;

    case WM_KEYDOWN:
        if (HandleRendererKeyDown(wParam)) {
            break;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_KEYUP:
        if (HandleRendererKeyUp(wParam)) {
            break;
        }
        return DefWindowProc(hWnd, message, wParam, lParam);

    case WM_SIZE:
    {
        // Handle resizing the window
        if (wParam != SIZE_MINIMIZED) // Ignore if the window is minimized
        {
            // Adjust the size of the status bar to fit the bottom of the window
            RECT rcClient;
            GetClientRect(hWnd, &rcClient); // Get the new client area of the window

            // Reposition the status bar at the bottom of the window
            if (statusBarMgr.GetHandle()) // Ensure status bar handle is valid
            {
                SetWindowPos(statusBarMgr.GetHandle(), nullptr, 0, 0, rcClient.right, 0, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE);
            }

            // Resize other components as needed (e.g., taskbar)
            statusBarMgr.Resize(); // Make sure your TaskBarMgr has a Resize function to handle resizing if needed
            LayoutBallControls(hWnd);
            {
                std::lock_guard<std::mutex> lock(ballConfigMutex);
                g_rendererResizeRequested = true;
            }
        }
    }
    break;
    case WM_ACTIVATE:
        isFocused = (wParam != WA_INACTIVE);
        if (!isFocused) {
            ClearBgeVectorShipInputState();
        }

        // Update instance count when window becomes active
        if (isFocused && !g_playerRuntimeMode)
        {
            int instanceCount = g_sharedData ? g_sharedData->instanceCount : 0; // Get the updated count from shared memory or IPC
            statusBarMgr.UpdateInstanceCount(instanceCount); // Update the status bar with the new count
        }
        break;

    case WM_KILLFOCUS:
        ClearBgeVectorShipInputState();
        break;

    case WM_TIMER:
        AdvanceSoundSlotLoop();
        if (g_isController) {
            ProcessControllerUiAutomation();
            SyncControllerControls();
        }
        break;

    case WM_COPYDATA:
        if (g_isController) {
            return HandleControllerTelemetryCopyData(reinterpret_cast<COPYDATASTRUCT*>(lParam)) ? TRUE : FALSE;
        }
        return HandleWorkerCommandCopyData(reinterpret_cast<COPYDATASTRUCT*>(lParam)) ? TRUE : FALSE;

    case WM_PAINT:
    {
        if (CurrentProcessOwnsGameLoop()) {
            PAINTSTRUCT ps;
            BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;
        }

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Double buffering to prevent flickering
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        HBRUSH hbrBkGnd = CreateSolidBrush(RGB(255, 255, 255)); // White background
        FillRect(hdcMem, &ps.rcPaint, hbrBkGnd);
        DeleteObject(hbrBkGnd);

        if (!g_isController && g_showMutexLine) {  // Pass A: gated by bge.toml [show_mutex_line]
            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0)); // Red line, 2 pixels wide
            HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);

            MoveToEx(hdcMem, static_cast<int>(lineX), static_cast<int>(lineY), NULL);
            LineTo(hdcMem, 200, 200);

            SelectObject(hdcMem, hOldPen);
            DeleteObject(hPen);
        }

        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_APP: // Custom message identifier for tray icon
    {
        if (lParam == WM_RBUTTONDOWN) // Right-click on the tray icon
        {
            taskBarMgr.ShowContextMenu(); // Use TaskBarMgr to show the context menu
        }
        else if (lParam == WM_LBUTTONDOWN) // Left-click on the tray icon
        {
            ShowWindow(hWnd, SW_RESTORE);
            taskBarMgr.RemoveTrayIcon();
            isPaused = false;
        }
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        if (wmId == IDC_BGE_CONTROLLER_HISTORY && HIWORD(wParam) == LBN_SELCHANGE) {
            UpdateControllerHistoryDetailFromSelection();
            break;
        }
        if (wmId >= IDC_BGE_CONTROLLER_TARGET_BASE && wmId < IDC_BGE_CONTROLLER_TARGET_BASE + BGE_CONTROLLER_ARTIFACT_COUNT) {
            SelectControllerArtifact(wmId - IDC_BGE_CONTROLLER_TARGET_BASE);
            break;
        }
        if (wmId >= IDC_BGE_CONTROLLER_LAUNCH_BASE && wmId < IDC_BGE_CONTROLLER_LAUNCH_BASE + BGE_CONTROLLER_ARTIFACT_COUNT) {
            LaunchControllerArtifact(wmId - IDC_BGE_CONTROLLER_LAUNCH_BASE);
            break;
        }
        if (wmId >= IDC_BGE_CONTROLLER_INSPECT_BASE && wmId < IDC_BGE_CONTROLLER_INSPECT_BASE + BGE_CONTROLLER_ARTIFACT_COUNT) {
            InspectControllerArtifact(wmId - IDC_BGE_CONTROLLER_INSPECT_BASE);
            break;
        }
        if (wmId >= IDC_BGE_CONTROLLER_HIDE_BASE && wmId < IDC_BGE_CONTROLLER_HIDE_BASE + BGE_CONTROLLER_ARTIFACT_COUNT) {
            HideControllerArtifact(wmId - IDC_BGE_CONTROLLER_HIDE_BASE);
            break;
        }
        if (wmId == IDC_BGE_OBJECT_GROUP_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
            SelectObjectGroupFromControls();
            break;
        }
        if (wmId == IDC_BGE_GHOST_GROUP_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
            SelectGhostGroupFromControls();
            break;
        }
        if (wmId >= IDC_BGE_OBJECT_SLOT_BASE && wmId < IDC_BGE_OBJECT_SLOT_BASE + BGE_OBJECT_SLOT_COUNT) {
            SelectObjectSlotFromControls(wmId - IDC_BGE_OBJECT_SLOT_BASE);
            break;
        }
        if (wmId >= IDC_BGE_SOUND_SLOT_BASE && wmId < IDC_BGE_SOUND_SLOT_BASE + BGE_OBJECT_SLOT_COUNT) {
            SelectSoundSlotFromControls(wmId - IDC_BGE_SOUND_SLOT_BASE);
            break;
        }

        switch (wmId)
        {
        case IDC_BGE_ADD_BALL:
            AddBallFromControls();
            break;

        case IDC_BGE_START_ANIMATION:
            StartAnimationFromControls();
            break;

        case IDC_BGE_STOP_ANIMATION:
            StopAnimationFromControls();
            break;

        case IDC_BGE_ADD_OBJECT_GROUP:
            AddObjectGroupFromControls();
            break;

        case IDC_BGE_TOGGLE_GHOST:
            ToggleGhostGroupFromControls();
            break;

        case IDC_BGE_SET_PLAYER:
            SetMainPlayerFromControls();
            break;

        case IDC_BGE_OPEN_MAPPING:
            ShowMappingWindow();
            break;

        case IDC_BGE_APPLY_VECTOR:
            ApplyVectorFromControls();
            break;

        case IDC_BGE_APPLY_COLOR:
            ApplyColorFromControls();
            break;

        case IDC_BGE_LOAD_BACKGROUND:
            LoadBackgroundFromDialog();
            break;

        case IDC_BGE_RUN_COMMAND:
            ExecuteCommandBarInput();
            break;

        case IDC_BGE_ASTEROID_GAME_WORKER:
            ExecuteAsteroidCommandFromControls(L"asteroid game");
            break;

        case IDC_BGE_ASTEROID_FIRE:
            ExecuteAsteroidCommandFromControls(L"asteroid fire");
            break;

        case IDC_BGE_ASTEROID_HYPERSPACE:
            ExecuteAsteroidCommandFromControls(L"asteroid hyperspace");
            break;

        case IDC_BGE_ASTEROID_PAUSE:
            ExecuteAsteroidCommandFromControls(L"asteroid pause");
            break;

        case IDC_BGE_ASTEROID_RESUME:
            ExecuteAsteroidCommandFromControls(L"asteroid resume");
            break;

        case IDC_BGE_ASTEROID_TITLE:
            ExecuteAsteroidCommandFromControls(L"asteroid title");
            break;

        case IDC_BGE_ASTEROID_HUD:
            ExecuteAsteroidCommandFromControls(L"asteroid hud");
            break;

        case IDC_BGE_ASTEROID_COMMANDS:
            ExecuteAsteroidCommandFromControls(L"asteroid commands");
            break;

        case IDC_BGE_OPEN_HISTORY:
            ShowControllerHistoryWindow();
            break;

        case IDC_BGE_ASTEROID_GAME_PRESET:
            LoadAsteroidGameFromController();
            break;

        case IDC_BGE_RENDERER:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                SwitchRendererFromControls();
            }
            break;

        case ID_TRAY_RESTORE:
            ShowWindow(hWnd, SW_RESTORE);
            taskBarMgr.RemoveTrayIcon();
            isPaused = false; // Resume the game loop
            break;

        case ID_TRAY_EXIT:
            SendMessage(hWnd, WM_CLOSE, 0, 0); // Close the application and controller-managed workers
            break;

        case IDM_EXIT:
            // File -> Exit: same teardown as tray Exit. Falls through to
            // WM_DESTROY -> PostQuitMessage -> main loop exits ->
            // shouldRun = false -> gameThread.join() -> mutex released.
            SendMessage(hWnd, WM_CLOSE, 0, 0);
            break;

        case IDM_ABOUT:
            // Help -> About: show the About dialog (its proc is already
            // defined as the About() callback above).
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_BGE_LAUNCH_STACK:
            LaunchBasicGameStack();
            break;

        case IDM_BGE_LAUNCH_GAME_LOOP:
            LaunchWorkerRole(L"bge.game-loop");
            break;

        case IDM_BGE_LAUNCH_SCENE_3D:
            LaunchWorkerRole(L"bge.scene-3d");
            break;

        case IDM_BGE_LAUNCH_IMAGES:
            LaunchWorkerRole(L"bge.images");
            break;

        case IDM_BGE_LAUNCH_SOUND:
            LaunchWorkerRole(L"bge.sound");
            break;

        case IDM_BGE_LAUNCH_SAMPLE_ONE:
            LaunchWorkerRole(L"bge.sample-game-one");
            break;

        case IDM_BGE_LAUNCH_SAMPLE_TWO:
            LaunchWorkerRole(L"bge.sample-game-two");
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
