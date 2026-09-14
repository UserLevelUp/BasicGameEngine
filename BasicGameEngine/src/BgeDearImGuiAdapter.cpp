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

    ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam);
    ImGuiIO& io = ImGui::GetIO();

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
        reportedBounds_.clear();
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
    std::vector<PendingAction> actions;
    CommandCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!visible_ || rendererBackend_ != RendererBackend::DirectX11) return;
        BeginFrame();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        actions.swap(pendingActions_);
        callback = commandCallback_;
    }
    DispatchActions(actions, callback);
}

void BgeDearImGuiAdapter::RenderDirectX12(ID3D12GraphicsCommandList* commandList)
{
    std::vector<PendingAction> actions;
    CommandCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!visible_ || rendererBackend_ != RendererBackend::DirectX12 || !commandList) return;
        BeginFrame();
        ImGui::Render();
        ID3D12DescriptorHeap* descriptorHeaps[] = { dx12SrvHeap_.Get() };
        commandList->SetDescriptorHeaps(1, descriptorHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
        actions.swap(pendingActions_);
        callback = commandCallback_;
    }
    DispatchActions(actions, callback);
}

void BgeDearImGuiAdapter::DispatchActions(const std::vector<PendingAction>& actions, const CommandCallback& callback)
{
    for (const auto& pending : actions) {
        const auto& action = pending.request.action;
        BgeUiActionResult result;
        // Until remote transport is wired, reject explicitly rather than executing locally.
        if (action.target != "local") {
            result = {BgeUiActionState::Failed, "Target unavailable: " + action.target};
        } else if (!callback) {
            result = {BgeUiActionState::Failed, "Command dispatcher unavailable"};
        } else {
            result = callback(std::wstring(action.id.begin(), action.id.end()),
                              std::wstring(action.command.begin(), action.command.end()));
        }
        pending.owner->CompleteAction(pending.request, result);
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
    ImGui::NewFrame();
    DrawSpikeSurface();
}

void BgeDearImGuiAdapter::SetWindowManager(std::shared_ptr<BgeUiWindowMgr> manager)
{
    std::lock_guard<std::mutex> lock(mutex_);
    windowManager_ = std::move(manager);
}

void BgeDearImGuiAdapter::AddWindowManager(std::shared_ptr<BgeUiWindowMgr> manager)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (manager && std::find(additionalWindows_.begin(), additionalWindows_.end(), manager) == additionalWindows_.end()) additionalWindows_.push_back(std::move(manager));
}

void BgeDearImGuiAdapter::ActivateItem(const std::shared_ptr<BgeUiWindowMgr>& owner, const std::string& id)
{
    if (!owner) return;
    BgeUiActionRequest request;
    if (owner->PrepareAction(id, request)) pendingActions_.push_back({owner, std::move(request)});
}

void BgeDearImGuiAdapter::DrawSpikeSurface()
{
    DrawWindowSurface(windowManager_, 0);
    float offset = 390;
    for (const auto& owner : additionalWindows_) { DrawWindowSurface(owner, offset); offset += 390; }
}

void BgeDearImGuiAdapter::DrawWindowSurface(const std::shared_ptr<BgeUiWindowMgr>& owner, float offset)
{
    if (!owner) return;
    const auto model = owner->Snapshot();
    ImGui::SetNextWindowPos(ImVec2(24.0f + offset, 164.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_Text, model.theme.text);
    if (!ImGui::Begin(model.title.c_str(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar)) {
        ImGui::End(); ImGui::PopStyleColor(); return;
    }
    const auto position = ImGui::GetWindowPos();
    const std::pair<int,int> location(static_cast<int>(position.x), static_cast<int>(position.y));
    auto previous = reportedWindowPositions_.find(model.title);
    if (diagnosticCallback_ && (previous == reportedWindowPositions_.end() || previous->second != location)) {
        reportedWindowPositions_[model.title] = location;
        diagnosticCallback_(L"bge.event.command-ui.panel.position title=" + std::wstring(model.title.begin(),model.title.end())
            + L" x=" + std::to_wstring(location.first) + L" y=" + std::to_wstring(location.second));
    }
    ImGui::TextUnformatted(kRendererName);
    ImGui::Separator();
    if (ImGui::BeginMenuBar()) {
        for (const auto& menu : model.menus) {
            const bool open = ImGui::BeginMenu(menu.label.c_str());
            ReportItemBounds("menu." + menu.id);
            if (open) {
                for (const auto& item : menu.children) {
                    BgeUiActionViewModel binding;
                    const bool enabled = owner->Resolve(item.id, binding);
                    ImGui::PushID(item.id.c_str());
                    if (ImGui::MenuItem(item.label.c_str(), nullptr, false, enabled)) ActivateItem(owner, item.id);
                    ReportItemBounds("menu-item." + item.id);
                    ImGui::PopID();
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMenuBar();
    }
    // Ungrouped actions are supplied by the manager in display order.
    std::vector<BgeUiActionViewModel> ungrouped;
    for (const auto& item : model.actions) if (item.group.empty()) ungrouped.push_back(item);
    if (!ungrouped.empty()) {
        const auto& item = ungrouped.front();
        ImGui::BeginDisabled(!item.enabled);
        if (ImGui::Button(item.label.c_str())) ActivateItem(owner, item.id);
        ImGui::EndDisabled();
        if (!buttonBoundsReported_ && diagnosticCallback_) {
            const ImVec2 minimum = ImGui::GetItemRectMin();
            const ImVec2 maximum = ImGui::GetItemRectMax();
            diagnosticCallback_(L"bge.event.command-ui.button.bounds min="
                + std::to_wstring(static_cast<int>(minimum.x)) + L"," + std::to_wstring(static_cast<int>(minimum.y))
                + L" max=" + std::to_wstring(static_cast<int>(maximum.x)) + L"," + std::to_wstring(static_cast<int>(maximum.y)));
            buttonBoundsReported_ = true;
        }
    }
    if (ungrouped.size() > 1) {
        const BgeUiActionViewModel* selected = &ungrouped[1];
        for (const auto& item : ungrouped) if (item.id == model.selectedAction) selected = &item;
        if (ImGui::BeginCombo("Command", selected->label.c_str())) {
            for (size_t i = 1; i < ungrouped.size(); ++i) {
                const auto& item = ungrouped[i];
                ImGui::BeginDisabled(!item.enabled);
                if (ImGui::Selectable(item.label.c_str(), item.id == selected->id)) {
                    owner->Select(item.id); ActivateItem(owner, item.id);
                }
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::Separator(); ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.5f, 0.85f, 1.0f, 1.0f), "BGE Glass Control Center");
    ImGui::Spacing();
    auto* drawList = ImGui::GetWindowDrawList();
    for (const auto& group : model.groups) {
        ImGui::PushID(group.id.c_str());
        DrawGlassButtonGroup(drawList, group, [this, owner](const std::string& id, const std::string&) { ActivateItem(owner, id); });
        ImGui::PopID();
    }
    ImGui::Separator();
    if (!model.lastActionId.empty()) {
        ImGui::TextWrapped("%s: %s", model.lastResult.Label(), model.lastResult.message.c_str());
    }
    ImGui::TextUnformatted("Esc closes the spike.");
    ImGui::End();
    ImGui::PopStyleColor();
}

void BgeDearImGuiAdapter::ReportItemBounds(const std::string& id)
{
    if (!diagnosticCallback_ || !reportedBounds_.insert(id).second) return;
    const auto minimum = ImGui::GetItemRectMin();
    const auto maximum = ImGui::GetItemRectMax();
    diagnosticCallback_(L"bge.event.command-ui.item.bounds id=" + std::wstring(id.begin(), id.end())
        + L" min=" + std::to_wstring(static_cast<int>(minimum.x)) + L"," + std::to_wstring(static_cast<int>(minimum.y))
        + L" max=" + std::to_wstring(static_cast<int>(maximum.x)) + L"," + std::to_wstring(static_cast<int>(maximum.y)));
}
