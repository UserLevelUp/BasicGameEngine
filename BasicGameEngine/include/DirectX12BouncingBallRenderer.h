#pragma once

#include <array>
#include <string>
#include <vector>
#include "BgeScenePrimitives.h"
#include "DirectXIncludes.h"

class DirectX12BouncingBallRenderer {
public:
    DirectX12BouncingBallRenderer() = default;
    ~DirectX12BouncingBallRenderer();

    DirectX12BouncingBallRenderer(const DirectX12BouncingBallRenderer&) = delete;
    DirectX12BouncingBallRenderer& operator=(const DirectX12BouncingBallRenderer&) = delete;

    bool Initialize(HWND hWnd);
    void Shutdown();
    void Resize();
    void Tick(double deltaMilliseconds);
    void Render();
    void AddBall();
    void SetAnimationRunning(bool running);
    void SetInitialVelocity(float velocityX, float velocityY);
    void SetBallColor(float red, float green, float blue);
    void SelectObjectSlot(int slotIndex);
    void SetObjectSelectionActive(bool active);
    void SetObjectSlotState(int slotIndex, const BgeObjectSlotState& slot);
    void SetGhostObjectSlotState(int slotIndex, const BgeObjectSlotState& slot);
    bool LoadBackgroundImage(const std::wstring& path);

    bool IsInitialized() const { return initialized_; }
    bool HasBall() const;
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> ObjectSlotStates() const { return slots_; }
    const std::wstring& LastError() const { return lastError_; }

private:
    static constexpr UINT FrameCount = 2;

    bool CreateDeviceAndSwapChain();
    bool CreateRenderTargets();
    bool CreateShadersAndPipeline();
    bool CreateCommandObjects();
    bool CreateVertexBuffer();
    bool CreateFenceObjects();
    void BuildBallVertices(BgeColorVertex* vertices, int& vertexCount) const;
    void AddVectorArrow(BgeColorVertex* vertices, int& vertexCount, const BgeObjectSlotState& slot) const;
    void ResetBallPosition(BgeObjectSlotState& slot);
    void WaitForGpu();
    void SetError(const wchar_t* context, HRESULT result);
    UINT ClientWidth() const;
    UINT ClientHeight() const;

    HWND hWnd_ = nullptr;
    bool initialized_ = false;
    bool animationRunning_ = false;
    std::wstring lastError_;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> renderTargetHeap_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> renderTargets_;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, FrameCount> commandAllocators_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

    UINT renderTargetDescriptorSize_ = 0;
    UINT frameIndex_ = 0;
    UINT64 fenceValue_ = 1;
    HANDLE fenceEvent_ = nullptr;
    UINT8* vertexBufferData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> slots_{};
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> ghostSlots_{};
    std::vector<BgeColorVertex> backgroundVertices_;
    int selectedSlot_ = 0;
    bool objectSelectionActive_ = true;
};