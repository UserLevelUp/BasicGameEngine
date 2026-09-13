#include "../include/BgeDearImGuiAdapter.h"
#include "../include/BgeGlassUi.h"

#include <array>

#include "../../third_party/dear-imgui/imgui.h"
#include "../../third_party/dear-imgui/backends/imgui_impl_dx11.h"
#include "../../third_party/dear-imgui/backends/imgui_impl_dx12.h"
#include "../../third_party/dear-imgui/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace {
constexpr UINT kDx12ImGuiDescriptorCount = 32;
constexpr const char* kRendererName = "BGE Command UI U0";
}

BgeDearImGuiAdapter::~BgeDearImGuiAdapter()
{
    Shutdown();
}

bool BgeDearImGuiAdapter::Initialize(HWND hWnd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return true;
    }
    if (!hWnd) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(hWnd)) {
        ImGui::DestroyContext();
        return false;
    }

    hWnd_ = hWnd;
    initialized_ = true;
    return true;
}

void BgeDearImGuiAdapter::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        return;
    }
    DetachRendererLocked();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    hWnd_ = nullptr;
    initialized_ = false;
    visible_ = false;
}

void BgeDearImGuiAdapter::AttachDirectX11(ID3D11Device* device, ID3D11DeviceContext* context)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !device || !context) {
        return;
    }
    if (rendererBackend_ == RendererBackend::DirectX11) {
        return;
    }
    DetachRendererLocked();
    if (ImGui_ImplDX11_Init(device, context)) {
        rendererBackend_ = RendererBackend::DirectX11;
    }
}

void BgeDearImGuiAdapter::AttachDirectX12(ID3D12Device* device, ID3D12CommandQueue* commandQueue, DXGI_FORMAT renderTargetFormat)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !device || !commandQueue) {
        return;
    }
    if (rendererBackend_ == RendererBackend::DirectX12) {
        return;
    }
    DetachRendererLocked();

    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDescription.NumDescriptors = kDx12ImGuiDescriptorCount;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(dx12SrvHeap_.GetAddressOf())))) {
        dx12SrvHeap_.Reset();
        return;
    }

    dx12DescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    dx12NextDescriptor_ = 0;
    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = device;
    initInfo.CommandQueue = commandQueue;
    initInfo.NumFramesInFlight = 2;
    initInfo.RTVFormat = renderTargetFormat;
    initInfo.SrvDescriptorHeap = dx12SrvHeap_.Get();
    initInfo.UserData = this;
    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle) {
        static_cast<BgeDearImGuiAdapter*>(info->UserData)->AllocateDx12Descriptor(cpuHandle, gpuHandle);
    };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};
    if (ImGui_ImplDX12_Init(&initInfo)) {
        rendererBackend_ = RendererBackend::DirectX12;
        return;
    }

    dx12SrvHeap_.Reset();
    dx12DescriptorSize_ = 0;
}

void BgeDearImGuiAdapter::DetachRenderer()
{
    std::lock_guard<std::mutex> lock(mutex_);
    DetachRendererLocked();
}

void BgeDearImGuiAdapter::DetachRendererLocked()
{
    if (rendererBackend_ == RendererBackend::DirectX11) {
        ImGui_ImplDX11_Shutdown();
    }
    else if (rendererBackend_ == RendererBackend::DirectX12) {
        ImGui_ImplDX12_Shutdown();
    }
    rendererBackend_ = RendererBackend::None;
    dx12SrvHeap_.Reset();
    dx12DescriptorSize_ = 0;
    dx12NextDescriptor_ = 0;
}

bool BgeDearImGuiAdapter::HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static thread_local bool s_inHandler = false;
    if (s_inHandler) {
        return false;
    }
    struct ReentrancyGuard {
        bool& flag;
        ReentrancyGuard(bool& f) : flag(f) { flag = true; }
        ~ReentrancyGuard() { flag = false; }
    } guard(s_inHandler);

    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !visible_) {
        return false;
    }

    if (message == WM_MOUSELEAVE || message == WM_CAPTURECHANGED) {
        return true;
    }

    if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) {
        lastMouseX_ = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        lastMouseY_ = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        ImGui::GetIO().AddMousePosEvent(lastMouseX_, lastMouseY_);
    }

    ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);
    ImGuiIO& io = ImGui::GetIO();

    if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) {
        lastMouseX_ = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        lastMouseY_ = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        io.AddMousePosEvent(lastMouseX_, lastMouseY_);
    }
    const bool keyboardMessage = message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
    const bool mouseMessage = message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
    if (message == WM_KEYDOWN && wParam == VK_ESCAPE) {
        visible_ = false;
        return true;
    }
    return keyboardMessage || mouseMessage;
}

void BgeDearImGuiAdapter::SetVisible(bool visible)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (visible && !visible_) {
        buttonBoundsReported_ = false;
    }
    visible_ = initialized_ && visible;
}

bool BgeDearImGuiAdapter::IsVisible() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return visible_;
}

bool BgeDearImGuiAdapter::IsInitialized() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

void BgeDearImGuiAdapter::SetCommandCallback(CommandCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    commandCallback_ = std::move(callback);
}

void BgeDearImGuiAdapter::SetDiagnosticCallback(DiagnosticCallback callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    diagnosticCallback_ = std::move(callback);
}

void BgeDearImGuiAdapter::RenderDirectX11()

{

    std::vector<std::pair<std::wstring, std::wstring>> actionsToDispatch;

    {

        std::lock_guard<std::mutex> lock(mutex_);

        if (!visible_ || rendererBackend_ != RendererBackend::DirectX11) {

            return;

        }

        BeginFrame();

        ImGui::Render();

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        actionsToDispatch.swap(pendingActions_);

    }

    for (const auto& action : actionsToDispatch) {

        if (commandCallback_) {

            commandCallback_(action.first, action.second);

        }

    }

}

void BgeDearImGuiAdapter::RenderDirectX12(ID3D12GraphicsCommandList* commandList)

{

    std::vector<std::pair<std::wstring, std::wstring>> actionsToDispatch;

    {

        std::lock_guard<std::mutex> lock(mutex_);

        if (!visible_ || rendererBackend_ != RendererBackend::DirectX12 || !commandList) {

            return;

        }

        BeginFrame();

        ImGui::Render();

        ID3D12DescriptorHeap* descriptorHeaps[] = { dx12SrvHeap_.Get() };

        commandList->SetDescriptorHeaps(1, descriptorHeaps);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

        actionsToDispatch.swap(pendingActions_);

    }

    for (const auto& action : actionsToDispatch) {

        if (commandCallback_) {

            commandCallback_(action.first, action.second);

        }

    }

}

void BgeDearImGuiAdapter::AllocateDx12Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle)
{
    if (!dx12SrvHeap_ || dx12NextDescriptor_ >= kDx12ImGuiDescriptorCount) {
        *cpuHandle = {};
        *gpuHandle = {};
        return;
    }
    *cpuHandle = dx12SrvHeap_->GetCPUDescriptorHandleForHeapStart();
    *gpuHandle = dx12SrvHeap_->GetGPUDescriptorHandleForHeapStart();
    const SIZE_T offset = static_cast<SIZE_T>(dx12NextDescriptor_) * dx12DescriptorSize_;
    cpuHandle->ptr += offset;
    gpuHandle->ptr += offset;
    ++dx12NextDescriptor_;
}

void BgeDearImGuiAdapter::BeginFrame()
{
    if (rendererBackend_ == RendererBackend::DirectX11) {
        ImGui_ImplDX11_NewFrame();
    }
    else if (rendererBackend_ == RendererBackend::DirectX12) {
        ImGui_ImplDX12_NewFrame();
    }
    ImGui_ImplWin32_NewFrame();
    if (lastMouseX_ >= 0.0f && lastMouseY_ >= 0.0f) {
        ImGui::GetIO().AddMousePosEvent(lastMouseX_, lastMouseY_);
    }
    ImGui::NewFrame();
    DrawSpikeSurface();
}

void BgeDearImGuiAdapter::DrawSpikeSurface()
{
    ImGui::SetNextWindowPos(ImVec2(24.0f, 164.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("BGE Command UI Spike", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(kRendererName);
    ImGui::Separator();
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Resolution status")) {
                InvokeCommand(L"menu.resolution-status", L"resolution status");
            }
            ImGui::EndMenu();
        }
if (ImGui::BeginMenu("Modes")) {
    if (ImGui::MenuItem("Ball Mode")) {
        InvokeCommand(L"menu.mode.ball", L"mode ball");
    }
    if (ImGui::MenuItem("Arcade Mode")) {
        InvokeCommand(L"menu.mode.arcade", L"mode arcade");
    }
    ImGui::EndMenu();
}
if (ImGui::BeginMenu("Display")) {
    if (ImGui::MenuItem("1080p FHD")) {
        InvokeCommand(L"menu.res.1080", L"resolution 1920 1080");
    }
    if (ImGui::MenuItem("720p HD")) {
        InvokeCommand(L"menu.res.720", L"resolution 1280 720");
    }
    ImGui::EndMenu();
}
if (ImGui::BeginMenu("Tools")) {
    if (ImGui::MenuItem("Spawn 10 Balls")) {
        InvokeCommand(L"menu.tools.spawn10", L"spawn ball 10");
    }
    if (ImGui::MenuItem("Launch Warp")) {
        InvokeCommand(L"menu.tools.warp", L"warp bubble activate");
    }
    ImGui::EndMenu();
}
        ImGui::EndMenuBar();
    }
    if (ImGui::Button("Resolution status")) {
        InvokeCommand(L"button.resolution-status", L"resolution status");
    }
    if (!buttonBoundsReported_ && diagnosticCallback_) {
        const ImVec2 minimum = ImGui::GetItemRectMin();
        const ImVec2 maximum = ImGui::GetItemRectMax();
        diagnosticCallback_(L"bge.event.command-ui.button.bounds min="
            + std::to_wstring(static_cast<int>(minimum.x)) + L"," + std::to_wstring(static_cast<int>(minimum.y))
            + L" max=" + std::to_wstring(static_cast<int>(maximum.x)) + L"," + std::to_wstring(static_cast<int>(maximum.y)));
        buttonBoundsReported_ = true;
    }

    static int selection = 0;
    const std::array<const char*, 2> commands = { "resolution status", "plugin commands" };
    if (ImGui::BeginCombo("Command", commands[selection])) {
        for (int index = 0; index < static_cast<int>(commands.size()); ++index) {
            bool selected = selection == index;
            if (ImGui::Selectable(commands[index], selected)) {
                selection = index;
                InvokeCommand(L"dropdown.command", index == 0 ? L"resolution status" : L"plugin commands");
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    
    // =========================================================================
    // Glass Buttons & Menu Subsystems (bge.ui.buttons + bge.ui.menu)
    // 3-layer visual model: Base (crisp text), Middle (glass body), Top (overlay)
    // =========================================================================
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f), "BGE Glass Control Center (OpNode Host)");
    ImGui::Spacing();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    auto clickAdapter = [this](const std::string& actionId, const std::string& command) {
        std::wstring wAction(actionId.begin(), actionId.end());
        std::wstring wCmd(command.begin(), command.end());
        InvokeCommand(wAction, wCmd);
    };

    // Group 1: Game Modes (Clover Four-Leaf Reflection Overlay)
    BgeGlassButtonGroupDescriptor modesGroup;
    modesGroup.id = "group.modes";
    modesGroup.label = "Game Modes (Clover Overlay)";
    modesGroup.buttons = {
        { "mode.ball", "Ball Mode", "mode ball", "icon-ball", "group.modes", BgeGlassOverlayStyle::CloverFourLeaf, 0.28f },
        { "mode.arcade", "Arcade Mode", "mode arcade", "icon-rocket", "group.modes", BgeGlassOverlayStyle::CloverFourLeaf, 0.28f }
    };
    DrawGlassButtonGroup(drawList, modesGroup, clickAdapter);

    // Group 2: Display & Resolution (Four-Pane Lit Window Reflection Overlay)
    BgeGlassButtonGroupDescriptor displayGroup;
    displayGroup.id = "group.display";
    displayGroup.label = "Display & Resolution (4-Pane Window Overlay)";
    displayGroup.buttons = {
        { "res.1080", "1080p FHD", "resolution 1920 1080", "icon-display", "group.display", BgeGlassOverlayStyle::WindowFourPane, 0.26f },
        { "res.720", "720p HD", "resolution 1280 720", "icon-display", "group.display", BgeGlassOverlayStyle::WindowFourPane, 0.26f }
    };
    DrawGlassButtonGroup(drawList, displayGroup, clickAdapter);

    // Group 3: Custom Tools (Specular Sheen Overlay)
    BgeGlassButtonGroupDescriptor toolsGroup;
    toolsGroup.id = "group.tools";
    toolsGroup.label = "Custom Tools (Specular Sheen Overlay)";
    toolsGroup.buttons = {
        { "tools.spawn10", "Spawn 10 Balls", "spawn ball 10", "icon-plus", "group.tools", BgeGlassOverlayStyle::SpecularSheen, 0.30f },
        { "tools.warp", "Launch Warp", "warp bubble activate", "icon-portal", "group.tools", BgeGlassOverlayStyle::SpecularSheen, 0.30f }
    };
    DrawGlassButtonGroup(drawList, toolsGroup, clickAdapter);

    ImGui::Separator();
    ImGui::TextUnformatted("Texture thumbnail gate: pending BGE asset bridge");
    ImGui::TextUnformatted("Esc closes the spike.");
    ImGui::End();
}

void BgeDearImGuiAdapter::InvokeCommand(const std::wstring& actionId, const std::wstring& command)

{

    pendingActions_.push_back({ actionId, command });

}


