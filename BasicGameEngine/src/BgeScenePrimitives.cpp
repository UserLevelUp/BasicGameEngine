#include "../include/BgeScenePrimitives.h"

#include "../framework.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <wincodec.h>
#include <wrl/client.h>

namespace {
constexpr int kBackgroundColumns = 48;
constexpr int kBackgroundRows = 27;

void SetHresultError(const wchar_t* context, HRESULT result, std::wstring& error)
{
    std::wstringstream stream;
    stream << context << L" failed with HRESULT 0x" << std::hex << static_cast<unsigned long>(result);
    error = stream.str();
}

void AddQuad(std::vector<BgeColorVertex>& vertices, float left, float top, float right, float bottom, float red, float green, float blue)
{
    BgeColorVertex topLeft{ left, top, red, green, blue, 1.0f };
    BgeColorVertex topRight{ right, top, red, green, blue, 1.0f };
    BgeColorVertex bottomLeft{ left, bottom, red, green, blue, 1.0f };
    BgeColorVertex bottomRight{ right, bottom, red, green, blue, 1.0f };

    vertices.push_back(topLeft);
    vertices.push_back(bottomLeft);
    vertices.push_back(topRight);
    vertices.push_back(topRight);
    vertices.push_back(bottomLeft);
    vertices.push_back(bottomRight);
}
}

bool LoadBackgroundImageMesh(const std::wstring& path, std::vector<BgeColorVertex>& vertices, std::wstring& error)
{
    vertices.clear();
    error.clear();

    HRESULT coResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninitializeCom = SUCCEEDED(coResult);
    if (FAILED(coResult) && coResult != RPC_E_CHANGED_MODE) {
        SetHresultError(L"CoInitializeEx", coResult, error);
        return false;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(result)) {
        SetHresultError(L"CoCreateInstance IWICImagingFactory", result, error);
        if (uninitializeCom) CoUninitialize();
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    result = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    if (FAILED(result)) {
        SetHresultError(L"IWICImagingFactory::CreateDecoderFromFilename", result, error);
        if (uninitializeCom) CoUninitialize();
        return false;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result)) {
        SetHresultError(L"IWICBitmapDecoder::GetFrame", result, error);
        if (uninitializeCom) CoUninitialize();
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    result = frame->GetSize(&width, &height);
    if (FAILED(result) || width == 0 || height == 0) {
        SetHresultError(L"IWICBitmapFrameDecode::GetSize", FAILED(result) ? result : E_FAIL, error);
        if (uninitializeCom) CoUninitialize();
        return false;
    }

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    result = factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result)) {
        SetHresultError(L"IWICImagingFactory::CreateFormatConverter", result, error);
        if (uninitializeCom) CoUninitialize();
        return false;
    }

    result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(result)) {
        SetHresultError(L"IWICFormatConverter::Initialize", result, error);
        if (uninitializeCom) CoUninitialize();
        return false;
    }

    std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    result = converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(result)) {
        SetHresultError(L"IWICFormatConverter::CopyPixels", result, error);
        if (uninitializeCom) CoUninitialize();
        return false;
    }

    vertices.reserve(kBackgroundColumns * kBackgroundRows * 6);
    for (int row = 0; row < kBackgroundRows; ++row) {
        for (int column = 0; column < kBackgroundColumns; ++column) {
            UINT sampleX = (std::min)(width - 1, static_cast<UINT>((static_cast<double>(column) + 0.5) * width / kBackgroundColumns));
            UINT sampleY = (std::min)(height - 1, static_cast<UINT>((static_cast<double>(row) + 0.5) * height / kBackgroundRows));
            size_t offset = (static_cast<size_t>(sampleY) * width + sampleX) * 4u;

            float red = pixels[offset] / 255.0f;
            float green = pixels[offset + 1] / 255.0f;
            float blue = pixels[offset + 2] / 255.0f;

            float left = -1.0f + (static_cast<float>(column) / kBackgroundColumns) * 2.0f;
            float right = -1.0f + (static_cast<float>(column + 1) / kBackgroundColumns) * 2.0f;
            float top = 1.0f - (static_cast<float>(row) / kBackgroundRows) * 2.0f;
            float bottom = 1.0f - (static_cast<float>(row + 1) / kBackgroundRows) * 2.0f;
            AddQuad(vertices, left, top, right, bottom, red, green, blue);
        }
    }

    if (uninitializeCom) CoUninitialize();
    return true;
}