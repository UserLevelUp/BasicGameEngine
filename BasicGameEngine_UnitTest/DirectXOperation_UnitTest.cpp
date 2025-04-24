// DirectXOperation_UnitTest.cpp
#include "pch.h"
#include "CppUnitTest.h"
#include "../DX11Operation/DirectXOperation.h"
#include <Windows.h>  // Include Windows API header for LoadLibrary, FreeLibrary
#include <memory>
#include <iostream>
#include <filesystem>
#include "../OpNode/IOperate.h"  // Include IOperate from OpNode project

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DirectXOperationTests {

    LRESULT CALLBACK DummyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    inline HWND CreateDummyWindow(bool showWindow = false) {
        const wchar_t CLASS_NAME[] = L"DummyWindowClass";

        WNDCLASS wc = {};
        wc.lpfnWndProc = DummyWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = CLASS_NAME;
        RegisterClass(&wc);

        HWND hWnd = CreateWindowEx(
            0,
            CLASS_NAME,
            L"Dummy Window",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );

        if (showWindow)
            ShowWindow(hWnd, SW_SHOW);
        else
            ShowWindow(hWnd, SW_HIDE);

        return hWnd;
    }

    TEST_CLASS(DirectXOperationUnitTests)
    {
    public:
        TEST_METHOD(TestDirectXInitialization) {
            HWND hWnd = CreateDummyWindow(true);
            DirectXOperation dxOp;
            bool result = dxOp.Initialize(hWnd, 800, 600);
            Assert::IsTrue(result, L"DirectXOperation should initialize successfully.");

            // Optionally call Render to see if the device works.
            dxOp.Render();

            // Delay to allow visual inspection for a few seconds.
            Sleep(1000); // Wait for 3 seconds

            dxOp.Cleanup();
            DestroyWindow(hWnd);  // Clean up dummy window.
        }
    };
}
