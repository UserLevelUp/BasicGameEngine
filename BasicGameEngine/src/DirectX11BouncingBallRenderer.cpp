#include "../include/DirectX11BouncingBallRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace {
constexpr int kBallSegments = 48;
constexpr int kBackgroundVertexCapacity = 48 * 27 * 6;
constexpr int kArrowVertexCapacity = 9;
constexpr int kBallVertexCount = kBallSegments * 3 * BGE_OBJECT_SLOT_COUNT;
constexpr int kRingVertexCount = kBallSegments * 6 * ((BGE_OBJECT_SLOT_COUNT * 2) + 1);
constexpr int kMaxVertexCount = kBackgroundVertexCapacity + (kBallVertexCount * 2) + kRingVertexCount + kArrowVertexCapacity;
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

DirectX11BouncingBallRenderer::~DirectX11BouncingBallRenderer()
{
    Shutdown();
}

bool DirectX11BouncingBallRenderer::Initialize(HWND hWnd)
{
    hWnd_ = hWnd;
    if (!hWnd_) {
        lastError_ = L"DirectX 11 renderer has no window handle";
        return false;
    }

    if (!CreateDeviceAndSwapChain()) {
        return false;
    }
    if (!CreateRenderTarget()) {
        return false;
    }
    if (!CreateShaders()) {
        return false;
    }
    if (!CreateVertexBuffer()) {
        return false;
    }

    initialized_ = true;
    return true;
}

void DirectX11BouncingBallRenderer::Shutdown()
{
    initialized_ = false;
    if (context_) {
        context_->ClearState();
    }

    vertexBuffer_.Reset();
    inputLayout_.Reset();
    alphaBlendState_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
    renderTargetView_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
    hWnd_ = nullptr;
}

void DirectX11BouncingBallRenderer::Resize()
{
    if (!initialized_ || !swapChain_) {
        return;
    }

    UINT width = ClientWidth();
    UINT height = ClientHeight();
    if (width == 0 || height == 0) {
        return;
    }

    if (context_) {
        ID3D11RenderTargetView* nullTarget = nullptr;
        context_->OMSetRenderTargets(1, &nullTarget, nullptr);
    }
    renderTargetView_.Reset();

    HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        SetError(L"IDXGISwapChain::ResizeBuffers", hr);
        return;
    }

    CreateRenderTarget();
}

void DirectX11BouncingBallRenderer::Tick(double deltaMilliseconds)
{
    if (!initialized_ || !animationRunning_) {
        return;
    }

    UINT width = (std::max)(ClientWidth(), 1u);
    UINT height = (std::max)(ClientHeight(), 1u);
    float seconds = static_cast<float>(deltaMilliseconds / 1000.0);

    BgeEdgePolicy edgePolicy = BgeCurrentEdgePolicy();
    for (auto& slot : slots_) {
        if (!slot.visible || slot.isDeleted) {
            continue;
        }

        slot.x += slot.velocityX * seconds;
        slot.y += slot.velocityY * seconds;

        BgeObjectExtent extent{
            slot.radius,
            (std::max)(slot.radius, static_cast<float>(width) - slot.radius),
            kRenderTopInset + slot.radius,
            (std::max)(slot.radius, static_cast<float>(height) - slot.radius),
        };

        BgeApplyEdgePolicy(slot, extent, edgePolicy);
    }

    BgeUpdateCollisionFlags(slots_);
}

void DirectX11BouncingBallRenderer::Render()
{
    if (!initialized_ || !context_ || !renderTargetView_ || !vertexBuffer_) {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = context_->Map(vertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        SetError(L"ID3D11DeviceContext::Map", hr);
        return;
    }

    BgeColorVertex* vertices = static_cast<BgeColorVertex*>(mapped.pData);
    int vertexCount = 0;
    for (const auto& vertex : backgroundVertices_) {
        if (vertexCount >= kMaxVertexCount) break;
        vertices[vertexCount++] = vertex;
    }

    int ballVertexCount = 0;
    BuildBallVertices(vertices + vertexCount, ballVertexCount);
    vertexCount += ballVertexCount;
    context_->Unmap(vertexBuffer_.Get(), 0);

    std::array<float, 4> clearColor{ 0.05f, 0.07f, 0.10f, 1.0f };
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor.data());

    UINT stride = sizeof(BgeColorVertex);
    UINT offset = 0;
    ID3D11Buffer* vertexBuffers[] = { vertexBuffer_.Get() };
    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);
    if (alphaBlendState_) {
        float blendFactor[4]{};
        context_->OMSetBlendState(alphaBlendState_.Get(), blendFactor, 0xffffffff);
    }
    context_->RSSetViewports(1, &viewport_);
    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
    if (vertexCount > 0) {
        context_->Draw(static_cast<UINT>(vertexCount), 0);
    }
    swapChain_->Present(1, 0);
}

void DirectX11BouncingBallRenderer::AddBall()
{
    BgeObjectSlotState& slot = slots_[selectedSlot_];
    slot.visible = true;
    ResetBallPosition(slot);
}

void DirectX11BouncingBallRenderer::SetAnimationRunning(bool running)
{
    animationRunning_ = running;
}

void DirectX11BouncingBallRenderer::SetInitialVelocity(float velocityX, float velocityY)
{
    slots_[selectedSlot_].velocityX = velocityX;
    slots_[selectedSlot_].velocityY = velocityY;
}

void DirectX11BouncingBallRenderer::SetBallColor(float red, float green, float blue)
{
    slots_[selectedSlot_].colorR = (std::max)(0.0f, (std::min)(1.0f, red));
    slots_[selectedSlot_].colorG = (std::max)(0.0f, (std::min)(1.0f, green));
    slots_[selectedSlot_].colorB = (std::max)(0.0f, (std::min)(1.0f, blue));
}

void DirectX11BouncingBallRenderer::SelectObjectSlot(int slotIndex)
{
    selectedSlot_ = (std::max)(0, (std::min)(BGE_OBJECT_SLOT_COUNT - 1, slotIndex));
}

void DirectX11BouncingBallRenderer::SetObjectSelectionActive(bool active)
{
    objectSelectionActive_ = active;
}

void DirectX11BouncingBallRenderer::SetObjectSlotState(int slotIndex, const BgeObjectSlotState& slot)
{
    if (slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
        return;
    }
    slots_[slotIndex] = slot;
}

void DirectX11BouncingBallRenderer::SetGhostObjectSlotState(int slotIndex, const BgeObjectSlotState& slot)
{
    if (slotIndex < 0 || slotIndex >= BGE_OBJECT_SLOT_COUNT) {
        return;
    }
    ghostSlots_[slotIndex] = slot;
}

bool DirectX11BouncingBallRenderer::LoadBackgroundImage(const std::wstring& path)
{
    return LoadBackgroundImageMesh(path, backgroundVertices_, lastError_);
}

bool DirectX11BouncingBallRenderer::HasBall() const
{
    return slots_[selectedSlot_].visible;
}

bool DirectX11BouncingBallRenderer::CreateDeviceAndSwapChain()
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = ClientWidth();
    swapChainDesc.BufferDesc.Height = ClientHeight();
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hWnd_;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL createdFeatureLevel{};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        swapChain_.GetAddressOf(),
        device_.GetAddressOf(),
        &createdFeatureLevel,
        context_.GetAddressOf());

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            swapChain_.GetAddressOf(),
            device_.GetAddressOf(),
            &createdFeatureLevel,
            context_.GetAddressOf());
    }

    if (FAILED(hr)) {
        SetError(L"D3D11CreateDeviceAndSwapChain", hr);
        return false;
    }

    return true;
}

bool DirectX11BouncingBallRenderer::CreateRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) {
        SetError(L"IDXGISwapChain::GetBuffer", hr);
        return false;
    }

    hr = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView_.GetAddressOf());
    if (FAILED(hr)) {
        SetError(L"ID3D11Device::CreateRenderTargetView", hr);
        return false;
    }

    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>((std::max)(ClientWidth(), 1u));
    viewport_.Height = static_cast<float>((std::max)(ClientHeight(), 1u));
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    return true;
}

bool DirectX11BouncingBallRenderer::CreateShaders()
{
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;

    HRESULT hr = D3DCompile(
        kVertexShaderSource,
        strlen(kVertexShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "VSMain",
        "vs_4_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        vertexShaderBlob.GetAddressOf(),
        errors.GetAddressOf());
    if (FAILED(hr)) {
        SetError(L"D3DCompile vertex shader", hr);
        return false;
    }

    hr = D3DCompile(
        kPixelShaderSource,
        strlen(kPixelShaderSource),
        nullptr,
        nullptr,
        nullptr,
        "PSMain",
        "ps_4_0",
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        pixelShaderBlob.GetAddressOf(),
        errors.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetError(L"D3DCompile pixel shader", hr);
        return false;
    }

    hr = device_->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(), nullptr, vertexShader_.GetAddressOf());
    if (FAILED(hr)) {
        SetError(L"ID3D11Device::CreateVertexShader", hr);
        return false;
    }

    hr = device_->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(), nullptr, pixelShader_.GetAddressOf());
    if (FAILED(hr)) {
        SetError(L"ID3D11Device::CreatePixelShader", hr);
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device_->CreateInputLayout(
        inputElements,
        static_cast<UINT>(std::size(inputElements)),
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize(),
        inputLayout_.GetAddressOf());
    if (FAILED(hr)) {
        SetError(L"ID3D11Device::CreateInputLayout", hr);
        return false;
    }

    D3D11_BLEND_DESC blendDesc{};
    D3D11_RENDER_TARGET_BLEND_DESC& renderTargetBlend = blendDesc.RenderTarget[0];
    renderTargetBlend.BlendEnable = TRUE;
    renderTargetBlend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    renderTargetBlend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    renderTargetBlend.BlendOp = D3D11_BLEND_OP_ADD;
    renderTargetBlend.SrcBlendAlpha = D3D11_BLEND_ONE;
    renderTargetBlend.DestBlendAlpha = D3D11_BLEND_ZERO;
    renderTargetBlend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    renderTargetBlend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device_->CreateBlendState(&blendDesc, alphaBlendState_.GetAddressOf());
    if (FAILED(hr)) {
        SetError(L"ID3D11Device::CreateBlendState", hr);
        return false;
    }

    return true;
}

bool DirectX11BouncingBallRenderer::CreateVertexBuffer()
{
    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = sizeof(BgeColorVertex) * kMaxVertexCount;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device_->CreateBuffer(&bufferDesc, nullptr, vertexBuffer_.GetAddressOf());
    if (FAILED(hr)) {
        SetError(L"ID3D11Device::CreateBuffer", hr);
        return false;
    }

    return true;
}

void DirectX11BouncingBallRenderer::BuildBallVertices(BgeColorVertex* vertices, int& vertexCount) const
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
    auto appendRoundSlot = [&](const BgeObjectSlotState& slot, bool ghost) {
        if (!slot.visible || slot.isDeleted) {
            return;
        }

        float radius = ghost ? slot.radius * 1.08f : slot.radius;
        float alpha = ghost ? (std::min)(0.42f, slot.colorA * 0.55f) : slot.colorA;
        float centerRed = ghost ? 0.16f + slot.colorR * 0.18f : (std::min)(1.0f, slot.colorR + 0.15f);
        float centerGreen = ghost ? 0.16f + slot.colorG * 0.18f : (std::min)(1.0f, slot.colorG + 0.15f);
        float centerBlue = ghost ? 0.18f + slot.colorB * 0.22f : (std::min)(1.0f, slot.colorB + 0.15f);
        float edgeRed = ghost ? 0.10f + slot.colorR * 0.16f : slot.colorR;
        float edgeGreen = ghost ? 0.10f + slot.colorG * 0.16f : slot.colorG;
        float edgeBlue = ghost ? 0.12f + slot.colorB * 0.20f : slot.colorB;

        BgeColorVertex center{
            toNdcX(slot.x),
            toNdcY(slot.y),
            centerRed,
            centerGreen,
            centerBlue,
            alpha,
        };

        for (int segment = 0; segment < kBallSegments; ++segment) {
            float angle0 = (static_cast<float>(segment) / kBallSegments) * 2.0f * kPi;
            float angle1 = (static_cast<float>(segment + 1) / kBallSegments) * 2.0f * kPi;

            BgeColorVertex edge0{
                toNdcX(slot.x + std::cos(angle0) * radius),
                toNdcY(slot.y + std::sin(angle0) * radius),
                edgeRed,
                edgeGreen,
                edgeBlue,
                alpha,
            };
            BgeColorVertex edge1{
                toNdcX(slot.x + std::cos(angle1) * radius),
                toNdcY(slot.y + std::sin(angle1) * radius),
                edgeRed,
                edgeGreen,
                edgeBlue,
                alpha,
            };

            vertices[vertexCount++] = center;
            vertices[vertexCount++] = edge0;
            vertices[vertexCount++] = edge1;
        }
    };

    auto appendAsteroidSlot = [&](const BgeObjectSlotState& slot, bool ghost) {
        if (!slot.visible || slot.isDeleted) {
            return;
        }

        float radius = ghost ? slot.radius * 1.10f : slot.radius;
        float alpha = ghost ? (std::min)(0.38f, slot.colorA * 0.50f) : slot.colorA;
        float centerRed = ghost ? 0.16f + slot.colorR * 0.16f : (std::min)(1.0f, slot.colorR + 0.18f);
        float centerGreen = ghost ? 0.16f + slot.colorG * 0.16f : (std::min)(1.0f, slot.colorG + 0.16f);
        float centerBlue = ghost ? 0.18f + slot.colorB * 0.18f : (std::min)(1.0f, slot.colorB + 0.12f);
        float edgeRed = ghost ? 0.10f + slot.colorR * 0.14f : slot.colorR;
        float edgeGreen = ghost ? 0.10f + slot.colorG * 0.14f : slot.colorG;
        float edgeBlue = ghost ? 0.12f + slot.colorB * 0.16f : slot.colorB;

        BgeColorVertex center{ toNdcX(slot.x), toNdcY(slot.y), centerRed, centerGreen, centerBlue, alpha };
        for (int segment = 0; segment < BGE_ASTEROID_POINT_COUNT; ++segment) {
            int nextSegment = (segment + 1) % BGE_ASTEROID_POINT_COUNT;
            float angle0 = (static_cast<float>(segment) / BGE_ASTEROID_POINT_COUNT) * 2.0f * kPi;
            float angle1 = (static_cast<float>(nextSegment) / BGE_ASTEROID_POINT_COUNT) * 2.0f * kPi;
            float radius0 = radius * BGE_ASTEROID_RADIUS_PROFILE[static_cast<size_t>(segment)];
            float radius1 = radius * BGE_ASTEROID_RADIUS_PROFILE[static_cast<size_t>(nextSegment)];
            float shade0 = 0.80f + (segment % 3) * 0.07f;
            float shade1 = 0.80f + (nextSegment % 3) * 0.07f;

            BgeColorVertex edge0{
                toNdcX(slot.x + std::cos(angle0) * radius0),
                toNdcY(slot.y + std::sin(angle0) * radius0),
                (std::min)(1.0f, edgeRed * shade0),
                (std::min)(1.0f, edgeGreen * shade0),
                (std::min)(1.0f, edgeBlue * shade0),
                alpha,
            };
            BgeColorVertex edge1{
                toNdcX(slot.x + std::cos(angle1) * radius1),
                toNdcY(slot.y + std::sin(angle1) * radius1),
                (std::min)(1.0f, edgeRed * shade1),
                (std::min)(1.0f, edgeGreen * shade1),
                (std::min)(1.0f, edgeBlue * shade1),
                alpha,
            };

            vertices[vertexCount++] = center;
            vertices[vertexCount++] = edge0;
            vertices[vertexCount++] = edge1;
        }
    };

    auto appendObjectSlot = [&](const BgeObjectSlotState& slot, bool ghost) {
        if (slot.shape == BgeObjectShape::Asteroid) {
            appendAsteroidSlot(slot, ghost);
        }
        else {
            appendRoundSlot(slot, ghost);
        }
    };

    auto appendRing = [&](const BgeObjectSlotState& slot, float innerRadius, float outerRadius, float red, float green, float blue) {
        if (!slot.visible || slot.isDeleted) {
            return;
        }

        for (int segment = 0; segment < kBallSegments; ++segment) {
            float angle0 = (static_cast<float>(segment) / kBallSegments) * 2.0f * kPi;
            float angle1 = (static_cast<float>(segment + 1) / kBallSegments) * 2.0f * kPi;

            BgeColorVertex inner0{ toNdcX(slot.x + std::cos(angle0) * innerRadius), toNdcY(slot.y + std::sin(angle0) * innerRadius), red, green, blue, 1.0f };
            BgeColorVertex outer0{ toNdcX(slot.x + std::cos(angle0) * outerRadius), toNdcY(slot.y + std::sin(angle0) * outerRadius), red, green, blue, 1.0f };
            BgeColorVertex inner1{ toNdcX(slot.x + std::cos(angle1) * innerRadius), toNdcY(slot.y + std::sin(angle1) * innerRadius), red, green, blue, 1.0f };
            BgeColorVertex outer1{ toNdcX(slot.x + std::cos(angle1) * outerRadius), toNdcY(slot.y + std::sin(angle1) * outerRadius), red, green, blue, 1.0f };

            vertices[vertexCount++] = inner0;
            vertices[vertexCount++] = outer0;
            vertices[vertexCount++] = outer1;
            vertices[vertexCount++] = inner0;
            vertices[vertexCount++] = outer1;
            vertices[vertexCount++] = inner1;
        }
    };

    for (const auto& slot : ghostSlots_) {
        appendObjectSlot(slot, true);
    }
    for (const auto& slot : slots_) {
        appendObjectSlot(slot, false);
    }

    for (const auto& slot : slots_) {
        if (slot.deleteMarked) {
            appendRing(slot, slot.radius + 3.0f, slot.radius + 6.0f, 1.0f, 0.08f, 0.08f);
        }
        if (slot.collisionDetected) {
            appendRing(slot, slot.radius + 6.0f, slot.radius + 9.0f, 1.0f, 0.86f, 0.12f);
        }
    }

    if (objectSelectionActive_ && selectedSlot_ >= 0 && selectedSlot_ < BGE_OBJECT_SLOT_COUNT) {
        const BgeObjectSlotState& selectedSlot = slots_[selectedSlot_];
        appendRing(selectedSlot, selectedSlot.radius + 8.0f, selectedSlot.radius + 11.0f, 0.25f, 1.0f, 0.45f);
    }

    if (objectSelectionActive_ && slots_[selectedSlot_].visible && !slots_[selectedSlot_].isDeleted) {
        AddVectorArrow(vertices, vertexCount, slots_[selectedSlot_]);
    }
}

void DirectX11BouncingBallRenderer::AddVectorArrow(BgeColorVertex* vertices, int& vertexCount, const BgeObjectSlotState& slot) const
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

void DirectX11BouncingBallRenderer::SetError(const wchar_t* context, HRESULT hr)
{
    std::wstringstream stream;
    stream << context << L" failed with HRESULT 0x" << std::hex << static_cast<unsigned long>(hr);
    lastError_ = stream.str();
}

void DirectX11BouncingBallRenderer::ResetBallPosition(BgeObjectSlotState& slot)
{
    float width = static_cast<float>((std::max)(ClientWidth(), 1u));
    float height = static_cast<float>((std::max)(ClientHeight(), 1u));
    float slotOffset = static_cast<float>(selectedSlot_) * 24.0f;
    slot.x = (std::min)(width - slot.radius, (std::max)(slot.radius, width * 0.28f + slotOffset));
    slot.y = (std::max)(kRenderTopInset + slot.radius, height * 0.45f);
}

UINT DirectX11BouncingBallRenderer::ClientWidth() const
{
    RECT rect{};
    if (!hWnd_ || !GetClientRect(hWnd_, &rect)) {
        return 1;
    }
    return static_cast<UINT>((std::max)(rect.right - rect.left, 1L));
}

UINT DirectX11BouncingBallRenderer::ClientHeight() const
{
    RECT rect{};
    if (!hWnd_ || !GetClientRect(hWnd_, &rect)) {
        return 1;
    }
    return static_cast<UINT>((std::max)(rect.bottom - rect.top, 1L));
}