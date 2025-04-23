#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#include "pch.h"

// DirectXOperation.cpp
#include "DirectXOperation.h"
#include "../OpNode/OpNode.h" // Include the OpNode header to resolve incomplete type errors
#include <d3d11.h>
#include <dxgi.h>
#include <iostream>

DirectXOperation::DirectXOperation() {}
DirectXOperation::~DirectXOperation() {
    Cleanup();
}

std::string DirectXOperation::Symbol() const {
    return "DX11";
}

void DirectXOperation::Operate(std::shared_ptr<OpNode> node) {
    // For demonstration, print the operation.
    if (node) {
        std::cout << "DirectXOperation processing node: " << node->GetName() << std::endl; // Dereference node to access GetName
    }
}

bool DirectXOperation::Initialize(HWND hWnd, int width, int height) {
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;

    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &swapChain_,
        &device_,
        &featureLevel,
        &context_
    );
    if (FAILED(hr)) {
        std::cerr << "D3D11CreateDeviceAndSwapChain failed: " << hr << std::endl;
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) {
        std::cerr << "GetBuffer failed: " << hr << std::endl;
        return false;
    }

    hr = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView_);
    if (FAILED(hr)) {
        std::cerr << "CreateRenderTargetView failed: " << hr << std::endl;
        return false;
    }

    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);

    return true;
}

void DirectXOperation::Render() {
    FLOAT clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    swapChain_->Present(1, 0);
}

void DirectXOperation::Cleanup() {
    // ComPtr releases resources automatically.
}