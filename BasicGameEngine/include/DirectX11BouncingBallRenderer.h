#pragma once

#include <array>
#include <string>
#include <vector>
#include "BgeScenePrimitives.h"
#include "DirectXIncludes.h"

class BgeDearImGuiAdapter;

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
    void SetSceneOverlayText(const std::vector<BgeSceneOverlayText>& overlays);
    void SetSceneGeometry(const std::vector<BgeColorVertex>& vertices);
    bool LoadBackgroundImage(const std::wstring& path);
    void SetDearImGuiAdapter(BgeDearImGuiAdapter* adapter);

    bool IsInitialized() const { return initialized_; }
    bool HasBall() const;
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> ObjectSlotStates() const { return slots_; }
    const std::wstring& LastError() const { return lastError_; }

private:
    struct SpriteVertex {
        float x;
        float y;
        float u;
        float v;
        float r;
        float g;
        float b;
        float a;
    };

    bool CreateDeviceAndSwapChain();
    bool CreateRenderTarget();
    bool CreateShaders();
    bool CreateVertexBuffer();
    bool CreateSpriteShaders();
    bool CreateSpriteVertexBuffer();
    bool EnsureSpriteTextureLoaded(const std::wstring& path);
    int BuildSpriteVertices(SpriteVertex* vertices, int maxVertexCount) const;
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
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> noCullRasterizerState_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> spriteVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> spritePixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> spriteInputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> spriteVertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> spriteSamplerState_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> spriteTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spriteTextureView_;
    D3D11_VIEWPORT viewport_{};

    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> slots_{};
    std::array<BgeObjectSlotState, BGE_OBJECT_SLOT_COUNT> ghostSlots_{};
    std::vector<BgeColorVertex> backgroundVertices_;
    std::vector<BgeColorVertex> sceneGeometryVertices_;
    std::vector<BgeSceneOverlayText> sceneOverlayText_;
    std::wstring spriteTexturePath_;
    BgeDearImGuiAdapter* dearImGuiAdapter_ = nullptr;
    UINT spriteTextureWidth_ = 0;
    UINT spriteTextureHeight_ = 0;
    int selectedSlot_ = 0;
    bool objectSelectionActive_ = true;
};