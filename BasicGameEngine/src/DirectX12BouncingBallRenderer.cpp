#include "../include/DirectX12BouncingBallRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <sstream>

namespace {
constexpr int kBallSegments = 48;
constexpr int kBackgroundVertexCapacity = 48 * 27 * 6;
constexpr int kArrowVertexCapacity = 9;
constexpr int kBallVertexCount = kBallSegments * 3 * BGE_OBJECT_SLOT_COUNT;
constexpr int kMaxVertexCount = kBackgroundVertexCapacity + kBallVertexCount + kArrowVertexCapacity;
constexpr float kRenderTopInset = 140.0f;
constexpr float kPi = 3.14159265358979323846f;

const char* kVertexShaderSource = R"(
struct VSInput {
    float2 position : POSITION;
    float4 color : COLOR;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    output.position = float4(input.position, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}
)";

const char* kPixelShaderSource = R"(
struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
)";
}

DirectX12BouncingBallRenderer::~DirectX12BouncingBallRenderer()
{
    Shutdown();
}

bool DirectX12BouncingBallRenderer::Initialize(HWND hWnd)
{
    hWnd_ = hWnd;
    if (!hWnd_) {
        lastError_ = L"DirectX 12 renderer has no window handle";
        return false;
    }

    if (!CreateDeviceAndSwapChain()) return false;
    if (!CreateRenderTargets()) return false;
    if (!CreateShadersAndPipeline()) return false;
    if (!CreateCommandObjects()) return false;
    if (!CreateVertexBuffer()) return false;
    if (!CreateFenceObjects()) return false;

    initialized_ = true;
    return true;
}

void DirectX12BouncingBallRenderer::Shutdown()
{
    if (initialized_) {
        WaitForGpu();
    }
    initialized_ = false;

    if (vertexBuffer_ && vertexBufferData_) {
        vertexBuffer_->Unmap(0, nullptr);
    }
    vertexBufferData_ = nullptr;

    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    fence_.Reset();
    vertexBuffer_.Reset();
    commandList_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    for (auto& allocator : commandAllocators_) allocator.Reset();
    for (auto& target : renderTargets_) target.Reset();
    renderTargetHeap_.Reset();
    swapChain_.Reset();
    commandQueue_.Reset();
    device_.Reset();
    hWnd_ = nullptr;
}

void DirectX12BouncingBallRenderer::Resize()
{
    if (!initialized_ || !swapChain_) {
        return;
    }

    UINT width = ClientWidth();
    UINT height = ClientHeight();
    if (width == 0 || height == 0) {
        return;
    }

    WaitForGpu();
    for (auto& target : renderTargets_) target.Reset();

    HRESULT result = swapChain_->ResizeBuffers(FrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(result)) {
        SetError(L"IDXGISwapChain3::ResizeBuffers", result);
        return;
    }

    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
    CreateRenderTargets();
}

void DirectX12BouncingBallRenderer::Tick(double deltaMilliseconds)
{
    if (!initialized_ || !animationRunning_) {
        return;
    }

    UINT width = (std::max)(ClientWidth(), 1u);
    UINT height = (std::max)(ClientHeight(), 1u);
    float seconds = static_cast<float>(deltaMilliseconds / 1000.0);

    for (auto& slot : slots_) {
        if (!slot.visible) {
            continue;
        }

        slot.x += slot.velocityX * seconds;
        slot.y += slot.velocityY * seconds;

        float minX = slot.radius;
        float maxX = (std::max)(slot.radius, static_cast<float>(width) - slot.radius);
        float minY = kRenderTopInset + slot.radius;
        float maxY = (std::max)(slot.radius, static_cast<float>(height) - slot.radius);

        if (slot.x < minX) {
            slot.x = minX;
            slot.velocityX = std::abs(slot.velocityX);
        }
        else if (slot.x > maxX) {
            slot.x = maxX;
            slot.velocityX = -std::abs(slot.velocityX);
        }

        if (slot.y < minY) {
            slot.y = minY;
            slot.velocityY = std::abs(slot.velocityY);
        }
        else if (slot.y > maxY) {
            slot.y = maxY;
            slot.velocityY = -std::abs(slot.velocityY);
        }
    }
}

void DirectX12BouncingBallRenderer::Render()
{
    if (!initialized_ || !commandList_ || !vertexBufferData_) {
        return;
    }

    int vertexCount = 0;
    BgeColorVertex* vertices = reinterpret_cast<BgeColorVertex*>(vertexBufferData_);
    for (const auto& vertex : backgroundVertices_) {
        if (vertexCount >= kMaxVertexCount) break;
        vertices[vertexCount++] = vertex;
    }

    int ballVertexCount = 0;
    BuildBallVertices(vertices + vertexCount, ballVertexCount);
    vertexCount += ballVertexCount;

    HRESULT result = commandAllocators_[frameIndex_]->Reset();
    if (FAILED(result)) {
        SetError(L"ID3D12CommandAllocator::Reset", result);
        return;
    }

    result = commandList_->Reset(commandAllocators_[frameIndex_].Get(), pipelineState_.Get());
    if (FAILED(result)) {
        SetError(L"ID3D12GraphicsCommandList::Reset", result);
        return;
    }

    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);

    D3D12_RESOURCE_BARRIER toRenderTarget{};
    toRenderTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRenderTarget.Transition.pResource = renderTargets_[frameIndex_].Get();
    toRenderTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRenderTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toRenderTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &toRenderTarget);

    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle = renderTargetHeap_->GetCPUDescriptorHandleForHeapStart();
    renderTargetHandle.ptr += static_cast<SIZE_T>(frameIndex_) * renderTargetDescriptorSize_;

    std::array<float, 4> clearColor{ 0.04f, 0.05f, 0.08f, 1.0f };
    commandList_->OMSetRenderTargets(1, &renderTargetHandle, FALSE, nullptr);
    commandList_->ClearRenderTargetView(renderTargetHandle, clearColor.data(), 0, nullptr);

    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);
    if (vertexCount > 0) {
        commandList_->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);
    }

    D3D12_RESOURCE_BARRIER toPresent = toRenderTarget;
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &toPresent);

    result = commandList_->Close();
    if (FAILED(result)) {
        SetError(L"ID3D12GraphicsCommandList::Close", result);
        return;
    }

    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);
    swapChain_->Present(1, 0);
    WaitForGpu();
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

void DirectX12BouncingBallRenderer::AddBall()
{
    BgeObjectSlotState& slot = slots_[selectedSlot_];
    slot.visible = true;
    ResetBallPosition(slot);
}

void DirectX12BouncingBallRenderer::SetAnimationRunning(bool running)
{
    animationRunning_ = running;
}

void DirectX12BouncingBallRenderer::SetInitialVelocity(float velocityX, float velocityY)
{
    slots_[selectedSlot_].velocityX = velocityX;
    slots_[selectedSlot_].velocityY = velocityY;
}

void DirectX12BouncingBallRenderer::SetBallColor(float red, float green, float blue)
{
    slots_[selectedSlot_].colorR = (std::max)(0.0f, (std::min)(1.0f, red));
    slots_[selectedSlot_].colorG = (std::max)(0.0f, (std::min)(1.0f, green));
    slots_[selectedSlot_].colorB = (std::max)(0.0f, (std::min)(1.0f, blue));
}

void DirectX12BouncingBallRenderer::SelectObjectSlot(int slotIndex)
{
    selectedSlot_ = (std::max)(0, (std::min)(BGE_OBJECT_SLOT_COUNT - 1, slotIndex));
}

void DirectX12BouncingBallRenderer::SetObjectSlotState(int slotIndex, const BgeObjectSlotState& slot)
{
    if (slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
        return;
    }
    slots_[slotIndex] = slot;
}

bool DirectX12BouncingBallRenderer::LoadBackgroundImage(const std::wstring& path)
{
    return LoadBackgroundImageMesh(path, backgroundVertices_, lastError_);
}

bool DirectX12BouncingBallRenderer::HasBall() const
{
    return slots_[selectedSlot_].visible;
}

bool DirectX12BouncingBallRenderer::CreateDeviceAndSwapChain()
{
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT result = CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(result)) {
        SetError(L"CreateDXGIFactory1", result);
        return false;
    }

    result = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.GetAddressOf()));
    if (FAILED(result)) {
        Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
        result = factory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf()));
        if (SUCCEEDED(result)) {
            result = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.GetAddressOf()));
        }
    }

    if (FAILED(result)) {
        SetError(L"D3D12CreateDevice", result);
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    result = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(commandQueue_.GetAddressOf()));
    if (FAILED(result)) {
        SetError(L"ID3D12Device::CreateCommandQueue", result);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = ClientWidth();
    swapChainDesc.Height = ClientHeight();
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    result = factory->CreateSwapChainForHwnd(commandQueue_.Get(), hWnd_, &swapChainDesc, nullptr, nullptr, swapChain.GetAddressOf());
    if (FAILED(result)) {
        SetError(L"IDXGIFactory4::CreateSwapChainForHwnd", result);
        return false;
    }

    factory->MakeWindowAssociation(hWnd_, DXGI_MWA_NO_ALT_ENTER);
    result = swapChain.As(&swapChain_);
    if (FAILED(result)) {
        SetError(L"IDXGISwapChain1::As IDXGISwapChain3", result);
        return false;
    }

    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
    return true;
}

bool DirectX12BouncingBallRenderer::CreateRenderTargets()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = FrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    HRESULT result = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(renderTargetHeap_.GetAddressOf()));
    if (FAILED(result)) {
        SetError(L"ID3D12Device::CreateDescriptorHeap", result);
        return false;
    }

    renderTargetDescriptorSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE renderTargetHandle = renderTargetHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT frame = 0; frame < FrameCount; ++frame) {
        result = swapChain_->GetBuffer(frame, IID_PPV_ARGS(renderTargets_[frame].GetAddressOf()));
        if (FAILED(result)) {
            SetError(L"IDXGISwapChain3::GetBuffer", result);
            return false;
        }

        device_->CreateRenderTargetView(renderTargets_[frame].Get(), nullptr, renderTargetHandle);
        renderTargetHandle.ptr += renderTargetDescriptorSize_;
    }

    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>((std::max)(ClientWidth(), 1u));
    viewport_.Height = static_cast<float>((std::max)(ClientHeight(), 1u));
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = static_cast<LONG>((std::max)(ClientWidth(), 1u));
    scissorRect_.bottom = static_cast<LONG>((std::max)(ClientHeight(), 1u));
    return true;
}

bool DirectX12BouncingBallRenderer::CreateShadersAndPipeline()
{
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(result)) {
        SetError(L"D3D12SerializeRootSignature", result);
        return false;
    }

    result = device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf()));
    if (FAILED(result)) {
        SetError(L"ID3D12Device::CreateRootSignature", result);
        return false;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
    result = D3DCompile(kVertexShaderSource, strlen(kVertexShaderSource), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, vertexShader.GetAddressOf(), errors.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        SetError(L"D3DCompile vertex shader", result);
        return false;
    }

    result = D3DCompile(kPixelShaderSource, strlen(kPixelShaderSource), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0, pixelShader.GetAddressOf(), errors.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        SetError(L"D3DCompile pixel shader", result);
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;

    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc{};
    renderTargetBlendDesc.SrcBlend = D3D12_BLEND_ONE;
    renderTargetBlendDesc.DestBlend = D3D12_BLEND_ZERO;
    renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    renderTargetBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0] = renderTargetBlendDesc;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = FALSE;
    depthStencilDesc.StencilEnable = FALSE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
    pipelineDesc.InputLayout = { inputElements, static_cast<UINT>(std::size(inputElements)) };
    pipelineDesc.pRootSignature = rootSignature_.Get();
    pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.SampleMask = (std::numeric_limits<UINT>::max)();
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pipelineDesc.SampleDesc.Count = 1;

    result = device_->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(pipelineState_.GetAddressOf()));
    if (FAILED(result)) {
        SetError(L"ID3D12Device::CreateGraphicsPipelineState", result);
        return false;
    }

    return true;
}

bool DirectX12BouncingBallRenderer::CreateCommandObjects()
{
    for (auto& allocator : commandAllocators_) {
        HRESULT result = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(allocator.GetAddressOf()));
        if (FAILED(result)) {
            SetError(L"ID3D12Device::CreateCommandAllocator", result);
            return false;
        }
    }

    HRESULT result = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators_[frameIndex_].Get(), pipelineState_.Get(), IID_PPV_ARGS(commandList_.GetAddressOf()));
    if (FAILED(result)) {
        SetError(L"ID3D12Device::CreateCommandList", result);
        return false;
    }

    commandList_->Close();
    return true;
}

bool DirectX12BouncingBallRenderer::CreateVertexBuffer()
{
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeof(BgeColorVertex) * kMaxVertexCount;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT result = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(vertexBuffer_.GetAddressOf()));
    if (FAILED(result)) {
        SetError(L"ID3D12Device::CreateCommittedResource vertex buffer", result);
        return false;
    }

    D3D12_RANGE readRange{ 0, 0 };
    result = vertexBuffer_->Map(0, &readRange, reinterpret_cast<void**>(&vertexBufferData_));
    if (FAILED(result)) {
        SetError(L"ID3D12Resource::Map vertex buffer", result);
        return false;
    }

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.StrideInBytes = sizeof(BgeColorVertex);
    vertexBufferView_.SizeInBytes = sizeof(BgeColorVertex) * kMaxVertexCount;
    return true;
}

bool DirectX12BouncingBallRenderer::CreateFenceObjects()
{
    HRESULT result = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf()));
    if (FAILED(result)) {
        SetError(L"ID3D12Device::CreateFence", result);
        return false;
    }

    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
        lastError_ = L"CreateEventW for DirectX 12 fence failed";
        return false;
    }

    return true;
}

void DirectX12BouncingBallRenderer::BuildBallVertices(BgeColorVertex* vertices, int& vertexCount) const
{
    UINT width = (std::max)(ClientWidth(), 1u);
    UINT height = (std::max)(ClientHeight(), 1u);

    auto toNdcX = [width](float x) {
        return (x / static_cast<float>(width)) * 2.0f - 1.0f;
    };
    auto toNdcY = [height](float y) {
        return 1.0f - (y / static_cast<float>(height)) * 2.0f;
    };

    vertexCount = 0;
    for (const auto& slot : slots_) {
        if (!slot.visible) {
            continue;
        }

        BgeColorVertex center{
            toNdcX(slot.x),
            toNdcY(slot.y),
            (std::min)(1.0f, slot.colorR + 0.15f),
            (std::min)(1.0f, slot.colorG + 0.15f),
            (std::min)(1.0f, slot.colorB + 0.15f),
            1.0f,
        };

        for (int segment = 0; segment < kBallSegments; ++segment) {
            float angle0 = (static_cast<float>(segment) / kBallSegments) * 2.0f * kPi;
            float angle1 = (static_cast<float>(segment + 1) / kBallSegments) * 2.0f * kPi;

            BgeColorVertex edge0{
                toNdcX(slot.x + std::cos(angle0) * slot.radius),
                toNdcY(slot.y + std::sin(angle0) * slot.radius),
                slot.colorR,
                slot.colorG,
                slot.colorB,
                1.0f,
            };
            BgeColorVertex edge1{
                toNdcX(slot.x + std::cos(angle1) * slot.radius),
                toNdcY(slot.y + std::sin(angle1) * slot.radius),
                slot.colorR,
                slot.colorG,
                slot.colorB,
                1.0f,
            };

            vertices[vertexCount++] = center;
            vertices[vertexCount++] = edge0;
            vertices[vertexCount++] = edge1;
        }
    }

    if (slots_[selectedSlot_].visible) {
        AddVectorArrow(vertices, vertexCount, slots_[selectedSlot_]);
    }
}

void DirectX12BouncingBallRenderer::AddVectorArrow(BgeColorVertex* vertices, int& vertexCount, const BgeObjectSlotState& slot) const
{
    UINT width = (std::max)(ClientWidth(), 1u);
    UINT height = (std::max)(ClientHeight(), 1u);
    float tipX = slot.x + slot.velocityX * 0.35f;
    float tipY = slot.y + slot.velocityY * 0.35f;
    float dx = tipX - slot.x;
    float dy = tipY - slot.y;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0f) return;

    float ux = dx / length;
    float uy = dy / length;
    float px = -uy;
    float py = ux;
    float shaftWidth = 5.0f;
    float headLength = 18.0f;
    float baseX = tipX - ux * headLength;
    float baseY = tipY - uy * headLength;
    auto makeVertex = [width, height](float x, float y) {
        return BgeColorVertex{ (x / static_cast<float>(width)) * 2.0f - 1.0f, 1.0f - (y / static_cast<float>(height)) * 2.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    };

    BgeColorVertex s0 = makeVertex(slot.x + px * shaftWidth, slot.y + py * shaftWidth);
    BgeColorVertex s1 = makeVertex(slot.x - px * shaftWidth, slot.y - py * shaftWidth);
    BgeColorVertex s2 = makeVertex(baseX + px * shaftWidth, baseY + py * shaftWidth);
    BgeColorVertex s3 = makeVertex(baseX - px * shaftWidth, baseY - py * shaftWidth);
    BgeColorVertex h0 = makeVertex(tipX, tipY);
    BgeColorVertex h1 = makeVertex(baseX + px * headLength * 0.55f, baseY + py * headLength * 0.55f);
    BgeColorVertex h2 = makeVertex(baseX - px * headLength * 0.55f, baseY - py * headLength * 0.55f);

    vertices[vertexCount++] = s0;
    vertices[vertexCount++] = s1;
    vertices[vertexCount++] = s2;
    vertices[vertexCount++] = s2;
    vertices[vertexCount++] = s1;
    vertices[vertexCount++] = s3;
    vertices[vertexCount++] = h0;
    vertices[vertexCount++] = h1;
    vertices[vertexCount++] = h2;
}

void DirectX12BouncingBallRenderer::ResetBallPosition(BgeObjectSlotState& slot)
{
    float width = static_cast<float>((std::max)(ClientWidth(), 1u));
    float height = static_cast<float>((std::max)(ClientHeight(), 1u));
    float slotOffset = static_cast<float>(selectedSlot_) * 24.0f;
    slot.x = (std::min)(width - slot.radius, (std::max)(slot.radius, width * 0.28f + slotOffset));
    slot.y = (std::max)(kRenderTopInset + slot.radius, height * 0.45f);
}

void DirectX12BouncingBallRenderer::WaitForGpu()
{
    if (!commandQueue_ || !fence_ || !fenceEvent_) {
        return;
    }

    UINT64 signalValue = fenceValue_++;
    HRESULT result = commandQueue_->Signal(fence_.Get(), signalValue);
    if (FAILED(result)) {
        SetError(L"ID3D12CommandQueue::Signal", result);
        return;
    }

    if (fence_->GetCompletedValue() < signalValue) {
        result = fence_->SetEventOnCompletion(signalValue, fenceEvent_);
        if (FAILED(result)) {
            SetError(L"ID3D12Fence::SetEventOnCompletion", result);
            return;
        }
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void DirectX12BouncingBallRenderer::SetError(const wchar_t* context, HRESULT result)
{
    std::wstringstream stream;
    stream << context << L" failed with HRESULT 0x" << std::hex << static_cast<unsigned long>(result);
    lastError_ = stream.str();
}

UINT DirectX12BouncingBallRenderer::ClientWidth() const
{
    RECT rect{};
    if (!hWnd_ || !GetClientRect(hWnd_, &rect)) {
        return 1;
    }
    return static_cast<UINT>((std::max)(rect.right - rect.left, 1L));
}

UINT DirectX12BouncingBallRenderer::ClientHeight() const
{
    RECT rect{};
    if (!hWnd_ || !GetClientRect(hWnd_, &rect)) {
        return 1;
    }
    return static_cast<UINT>((std::max)(rect.bottom - rect.top, 1L));
}