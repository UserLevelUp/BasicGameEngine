#include "pch.h"
#include "VideoCard_UnitTest.h"

#include "CppUnitTest.h"
#include <dxgi.h>
#include <wrl/client.h>
#include <string>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Microsoft::WRL::ComPtr;

namespace BasicGameEngine_UnitTests
{
    TEST_CLASS(VideoCardTests)
    {
    public:
        TEST_METHOD(TestEnumerateVideoCards)
        {
            // Create a DXGI factory
            ComPtr<IDXGIFactory> factory;
            HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(factory.GetAddressOf()));
            Assert::IsTrue(SUCCEEDED(hr), L"Failed to create DXGIFactory.");

            // Enumerate the first adapter (video card)
            ComPtr<IDXGIAdapter> adapter;
            hr = factory->EnumAdapters(0, &adapter);
            Assert::IsTrue(SUCCEEDED(hr), L"No video adapter found.");

            // Retrieve the adapter description
            DXGI_ADAPTER_DESC adapterDesc = {};
            hr = adapter->GetDesc(&adapterDesc);
            Assert::IsTrue(SUCCEEDED(hr), L"Failed to get adapter description.");

            // Convert the wide string description to a wstring
            std::wstring adapterDescription(adapterDesc.Description);
            Logger::WriteMessage((L"Video Card: " + adapterDescription).c_str());

            // Assert that the description is not empty
            Assert::IsFalse(adapterDescription.empty(), L"Adapter description is empty.");
        }
    };
}