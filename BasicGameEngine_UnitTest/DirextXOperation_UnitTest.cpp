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

    // Simple window procedure for a dummy window.
    LRESULT CALLBACK DummyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    // Helper function to create a hidden window.
    HWND CreateDummyWindow() {
        const wchar_t CLASS_NAME[]  = L"DummyWindowClass";

        WNDCLASS wc = {};
        wc.lpfnWndProc   = DummyWndProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.lpszClassName = CLASS_NAME;
        RegisterClass(&wc);

        HWND hWnd = CreateWindowEx(
            0,                              // Optional window styles.
            CLASS_NAME,                     // Window class
            L"Dummy Window",                // Window text
            WS_OVERLAPPEDWINDOW,            // Window style
            CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
            nullptr,       // Parent window    
            nullptr,       // Menu
            GetModuleHandle(nullptr),       // Instance handle
            nullptr        // Additional application data
        );

        // Hide the window (for unit testing, no need to show it).
        ShowWindow(hWnd, SW_HIDE);
        return hWnd;
    }

    TEST_CLASS(DirectXOperationUnitTests)
    {
    public:
        TEST_METHOD(TestDirectXInitialization) {
            HWND hWnd = CreateDummyWindow();
            DirectXOperation dxOp;
            bool result = dxOp.Initialize(hWnd, 800, 600);
            Assert::IsTrue(result, L"DirectXOperation should initialize successfully.");

            // Optionally call Render to see if the device works.
            dxOp.Render();
            dxOp.Cleanup();
            DestroyWindow(hWnd);  // Clean up dummy window.
        }
    };
}
