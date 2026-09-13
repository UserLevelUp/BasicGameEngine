#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <set>

#include "DirectXIncludes.h"
#include "BgeUiWindowMgr.h"

class BgeDearImGuiAdapter {
public:
    using CommandCallback = std::function<void(const std::wstring& actionId, const std::wstring& command)>;
    using DiagnosticCallback = std::function<void(const std::wstring& message)>;

    BgeDearImGuiAdapter() = default;
    ~BgeDearImGuiAdapter();

    BgeDearImGuiAdapter(const BgeDearImGuiAdapter&) = delete;
    BgeDearImGuiAdapter& operator=(const BgeDearImGuiAdapter&) = delete;

    bool Initialize(HWND hWnd);
    void Shutdown();
    void AttachDirectX11(ID3D11Device* device, ID3D11DeviceContext* context);
    void AttachDirectX12(ID3D12Device* device, ID3D12CommandQueue* commandQueue, DXGI_FORMAT renderTargetFormat);
    void DetachRenderer();
    bool HandleWin32Message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void SetVisible(bool visible);
    bool IsVisible() const;
    bool IsInitialized() const;
    void SetWindowManager(std::shared_ptr<BgeUiWindowMgr> manager);
    void AddWindowManager(std::shared_ptr<BgeUiWindowMgr> manager);
    void SetCommandCallback(CommandCallback callback);
    void SetDiagnosticCallback(DiagnosticCallback callback);
    void RenderDirectX11();
    void RenderDirectX12(ID3D12GraphicsCommandList* commandList);

private:
    enum class RendererBackend {
        None,
        DirectX11,
        DirectX12,
    };

    void AllocateDx12Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle);
    void DetachRendererLocked();
    void BeginFrame();
    void DrawSpikeSurface();
    void InvokeCommand(const std::wstring& actionId, const std::wstring& command);

    std::shared_ptr<BgeUiWindowMgr> windowManager_;
    void ActivateItem(const std::shared_ptr<BgeUiWindowMgr>& owner, const std::string& id);
    void DrawWindowSurface(const std::shared_ptr<BgeUiWindowMgr>& owner, float offset);
    std::vector<std::shared_ptr<BgeUiWindowMgr>> additionalWindows_;
    void ReportItemBounds(const std::string& id);
    std::set<std::string> reportedBounds_;
    HWND hWnd_ = nullptr;
    mutable std::mutex mutex_;
    bool initialized_ = false;
    bool visible_ = false;
    RendererBackend rendererBackend_ = RendererBackend::None;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dx12SrvHeap_;
    UINT dx12DescriptorSize_ = 0;
    UINT dx12NextDescriptor_ = 0;
    bool buttonBoundsReported_ = false;
    CommandCallback commandCallback_;
    DiagnosticCallback diagnosticCallback_;
    std::vector<std::pair<std::wstring, std::wstring>> pendingActions_;
    float lastMouseX_ = -1.0f;
    float lastMouseY_ = -1.0f;
};
