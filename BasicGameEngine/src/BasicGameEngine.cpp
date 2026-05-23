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
#include "../include/TaskBarMgr.h"
#include "../include/WindowMutexMgr.h"
#include "../include/InterprocessCommMgr.h"  // Include the InterprocessCommMgr header
#include "../include/SharedMemoryData.h"      // Include the shared memory struct
#include "../include/UserPrivilegeMgr.h"

#include "../../OpNode/OpNode.h"
#include "../include/BgeGameModule.h"
#include "../include/BgeScenePrimitives.h"
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
constexpr int IDC_BGE_ASTEROID_R = 42920;
constexpr int IDC_BGE_ASTEROID_G = 42921;
constexpr int IDC_BGE_ASTEROID_B = 42922;
constexpr int IDC_BGE_ASTEROID_A = 42923;
constexpr int IDC_BGE_ASTEROID_APPLY_RGBA = 42924;
constexpr int IDC_BGE_ASTEROID_APPLY_SHAPE = 42925;
constexpr int IDC_BGE_ASTEROID_STATUS = 42926;
constexpr int IDC_BGE_ASTEROID_GAME_PRESET = 42927;
constexpr ULONG_PTR BGE_COPYDATA_WORKER_COMMAND = 0xB6E00001;
constexpr int BGE_CONTROLLER_ARTIFACT_COUNT = 6;
constexpr size_t BGE_CONTROLLER_HISTORY_LIMIT = 128;
constexpr size_t BGE_DELETE_HISTORY_LIMIT = 64;
constexpr float BGE_RENDER_TOP_INSET = 140.0f;
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

const std::array<BgeControllerArtifactSpec, BGE_CONTROLLER_ARTIFACT_COUNT> kControllerArtifacts = {{
    { L"Scene And Render", L"Game Loop", L"bge.game-loop", L"renderer, objects, animation", true },
    { L"Scene And Render", L"Scene 3D", L"bge.scene-3d", L"3D scene surface", false },
    { L"Scene And Render", L"Sample Game One", L"bge.sample-game-one", L"sample game plugin", false },
    { L"Scene And Render", L"Sample Game Two", L"bge.sample-game-two", L"sample game plugin", false },
    { L"Assets", L"Images", L"bge.images", L"backgrounds and image assets", false },
    { L"Audio", L"Sound", L"bge.sound", L"sound slots and playback", false },
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
void SelectSoundSlotFromControls(int slotIndex);
void AdvanceSoundSlotLoop();
void ExecuteCommandBarInput();
bool ExecuteCommandText(const std::wstring& commandText, std::wstring& statusText);
bool ExecuteControllerCommandText(const std::wstring& commandText, std::wstring& statusText);
bool QueueConstructionArtifactCommandsFromFile(const std::wstring& path, std::wstring& statusText);
bool SendControllerCommandToWorker(const std::wstring& role, const std::wstring& commandText, std::wstring& statusText);
void SetCommandStatus(const std::wstring& statusText);
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
void CycleEditMode(int direction);
bool SetEditModeFromText(const std::wstring& modeText);
std::wstring EditModeName(BgeEditMode mode);
float CurrentEditRateMultiplier();
std::wstring EditRateLabel();
void AdjustEditRate(int direction);
bool SetEditRateFromText(const std::wstring& rateText);
std::wstring TrimText(const std::wstring& value);
std::wstring WidenUtf8(const std::string& value);
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
    if (sharedData) {
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
    if (!CurrentProcessOwnsGameLoop() || y < static_cast<int>(BGE_RENDER_TOP_INSET)) {
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
    slot.y = ClampFloat(slot.y, BGE_RENDER_TOP_INSET + slot.radius, (std::max)(BGE_RENDER_TOP_INSET + slot.radius, height - slot.radius));
}

BgeGameViewport CurrentGameViewport()
{
    RECT client{};
    GetClientRect(g_hWnd, &client);
    BgeGameViewport viewport;
    viewport.width = static_cast<float>((std::max)(client.right - client.left, 640L));
    viewport.height = static_cast<float>((std::max)(client.bottom - client.top, 480L));
    viewport.playTop = BGE_RENDER_TOP_INSET;
    viewport.playHeight = (std::max)(240.0f, viewport.height - viewport.playTop);
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

void ParseRuntimeArgs()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return;
    }

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if ((arg == L"--worker" || arg == L"--role") && i + 1 < argc) {
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
        else if (arg.rfind(L"--game-file=", 0) == 0) {
            g_cliConstructionArtifactPaths.push_back(arg.substr(12));
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
    root->SetAttribute("environment", "basic-3d");
    if (role == "bge.game-loop") {
        root->AddOperation(std::make_shared<DirectX11BouncingBallOperation>());
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

void PlaceWorkerWindow(HWND workerWindow, const std::wstring& role)
{
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int width = 880;
    int height = 620;
    int x = workArea.right - width - 32;
    int y = workArea.top + 48;

    if (role == L"bge.sound" || role == L"bge.images") {
        width = 760;
        height = 480;
        x = workArea.right - width - 56;
        y = workArea.top + 96;
    }
    else if (role == L"bge.scene-3d") {
        x = workArea.right - width - 72;
        y = workArea.top + 72;
    }

    x = (std::max)(static_cast<int>(workArea.left) + 16, x);
    y = (std::max)(static_cast<int>(workArea.top) + 16, y);
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
            if (TickAsteroidGameMode(deltaMilliseconds)) {
                ApplyBallStateToRenderer();
            }
        }
        return;
    }
    if (g_directX11Renderer) {
        g_directX11Renderer->Tick(deltaMilliseconds);
        SyncObjectSlotsFromRenderer(g_directX11Renderer->ObjectSlotStates());
        if (TickAsteroidGameMode(deltaMilliseconds)) {
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

    if (command == L"asteroid-game" || command == L"asteroids" || command == L"game") {
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
        SetWindowTextW(g_commandEdit, L"");
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
        statusText = L"asteroid game | group/ghost/player | asteroid/edge | delete/undo | add/select | start/stop | mode/rate | mapping";
        logCommand("help");
        return true;
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

    if (command == L"score" || command == L"lives" || command == L"fire" || command == L"restart") {
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
            || subcommand == L"player" || subcommand == L"add" || subcommand == L"spawn" || subcommand == L"target" || subcommand == L"remove"
            || subcommand == L"clear" || subcommand == L"count") {
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

        statusText = L"Use: asteroid game | asteroid status | asteroid player set/select | asteroid add/remove | asteroid fire | asteroid score|lives | asteroid alpha/color/window";
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
        if (y < static_cast<int>(BGE_RENDER_TOP_INSET)) {
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

    int keyboardSlot = ObjectSlotIndexFromNumberKey(key);
    if (keyboardSlot >= 0) {
        return SelectObjectSlotFromKeyboard(keyboardSlot);
    }

    if (key == VK_OEM_3) {
        return FocusObjectGroupsFromKeyboard();
    }

    if (HandleAsteroidGameKeyDown(key)) {
        return true;
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
    text << L"  Rules: asteroid size controls score; ship collisions cost one life; zero lives or cleared asteroids stops the loop\r\n";
    text << L"  When object 1 is selected: A/D or Left/Right rotate, W/Up thrust, S/Down reverse, Space fires\r\n";
    text << L"  Bullets split large asteroids into smaller asteroids; edge policy switches to wrap\r\n\r\n";
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
    g_commandEdit = CreateControl(hWnd, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, IDC_BGE_COMMAND_EDIT, 42, 100, 512, 24);
    if (g_commandEdit) {
        g_commandEditOriginalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(g_commandEdit, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(CommandEditProc)));
    }
    g_runCommandButton = CreateControl(hWnd, L"BUTTON", L"Run", BS_PUSHBUTTON | WS_TABSTOP, IDC_BGE_RUN_COMMAND, 562, 100, 48, 24);
    g_commandStatus = CreateControl(hWnd, L"STATIC", L"help", 0, IDC_BGE_COMMAND_STATUS, 620, 104, 280, 20);
    UpdateEditModeStatus();
}

void LayoutBallControls(HWND hWnd)
{
    UNREFERENCED_PARAMETER(hWnd);
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

    WorkerSnapshot snapshot = GetWorkerSnapshot(L"bge.game-loop");
    if (!snapshot.running) {
        LaunchWorkerRole(L"bge.game-loop");
    }

    g_pendingControllerCommands.push_back(L"game-loop: asteroid game");

    SetCommandStatus(L"Asteroid Game queued");
    AddControllerHistory(L"Load preset -> Asteroid Game", ControllerBaseCli() + L" --launch-basic-game --ui-command " + QuoteArg(L"asteroid-game"));
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

    if (!g_cliControllerTarget.empty()) {
        int targetIndex = ResolveControllerArtifactIndex(g_cliControllerTarget);
        if (targetIndex >= 0) {
            SelectControllerArtifact(targetIndex);
        }
        g_cliControllerTarget.clear();
    }

    for (auto artifactPath = g_cliConstructionArtifactPaths.begin(); artifactPath != g_cliConstructionArtifactPaths.end();) {
        std::wstring statusText;
        QueueConstructionArtifactCommandsFromFile(*artifactPath, statusText);
        SetCommandStatus(statusText);
        artifactPath = g_cliConstructionArtifactPaths.erase(artifactPath);
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

    for (auto command = g_pendingControllerCommands.begin(); command != g_pendingControllerCommands.end();) {
        std::wstring targetRole = SelectedControllerRole();
        if (targetRole.empty() || !GetWorkerSnapshot(targetRole).running) {
            ++command;
            continue;
        }

        std::wstring statusText;
        bool executed = ExecuteControllerCommandText(*command, statusText);
        SetCommandStatus(statusText);
        if (executed || statusText.find(L"not running") == std::wstring::npos) {
            command = g_pendingControllerCommands.erase(command);
        }
        else {
            ++command;
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

        statusBarMgr.Update();

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
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_BASICGAMEENGINE);
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

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Initialize the taskbar manager
    taskBarMgr.Initialize(hWnd, hInstance);

    // Create the status bar
    statusBarMgr.Create(hWnd, hInstance);

    // Update the status bar with the privilege status
    std::wstring privilegeStatus = userPrivilegeMgr.GetUserPrivilege().GetPrivilegeStatus();
    statusBarMgr.UpdatePrivilegeStatus(privilegeStatus);

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

        // Update instance count when window becomes active
        if (isFocused)
        {
            int instanceCount = g_sharedData ? g_sharedData->instanceCount : 0; // Get the updated count from shared memory or IPC
            statusBarMgr.UpdateInstanceCount(instanceCount); // Update the status bar with the new count
        }
        break;

    case WM_TIMER:
        AdvanceSoundSlotLoop();
        if (g_isController) {
            ProcessControllerUiAutomation();
            SyncControllerControls();
        }
        break;

    case WM_COPYDATA:
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
