#pragma once

#include <array>
#include <string>
#include <vector>
#include "BgeScenePrimitives.h"
#include "DirectXIncludes.h"

class DirectX11BouncingBallRenderer {
public:
    DirectX11BouncingBallRenderer() = default;
    ~DirectX11BouncingBallRenderer();

    DirectX11BouncingBallRenderer(const DirectX11BouncingBallRenderer&) = delete;
    DirectX11BouncingBallRenderer& operator=(const DirectX11BouncingBallRenderer&) = delete;

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
    bool CreateDeviceAndSwapChain();
    bool CreateRenderTarget();
    bool CreateShaders();
    bool CreateVertexBuffer();
    void BuildBallVertices(BgeColorVertex* vertices, int& vertexCount) const;
    void AddVectorArrow(BgeColorVertex* vertices, int& vertexCount, const BgeObjectSlotState& slot) const;
    void SetError(const wchar_t* context, HRESULT hr);
    void ResetBallPosition(BgeObjectSlotState& slot);
    UINT ClientWidth() const;
    UINT ClientHeight() const;

    HWND hWnd_ = nullptr;
    bool initialized_ = false;
    bool ballVisible_ = false;
    bool animationRunning_ = false;
    std::wstring lastError_;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> alphaBlendState_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    D3D11_VIEWPORT viewport_{};

    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> slots_{};
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> ghostSlots_{};
    std::vector<BgeColorVertex> backgroundVertices_;
    int selectedSlot_ = 0;
    bool objectSelectionActive_ = true;
};