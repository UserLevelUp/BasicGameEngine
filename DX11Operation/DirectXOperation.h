// DirectXOperation.h
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "../OpNode/IOperate.h"
#include <string>

class DirectXOperation : public IOperate {
public:
    DirectXOperation();
    ~DirectXOperation();

    // IOperate interface implementation.
    std::string Symbol() const override;
    void Operate(std::shared_ptr<OpNode> node) override;

    // DirectX initialization and usage.
    bool Initialize(HWND hWnd, int width, int height);
    void Render();
    void Cleanup();

private:
    Microsoft::WRL::ComPtr<ID3D11Device>            device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>     context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain>          swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>    renderTargetView_;
};
