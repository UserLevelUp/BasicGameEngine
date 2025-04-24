#pragma once
#include <Windows.h>

LRESULT CALLBACK DummyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProc(hWnd, message, wParam, lParam);
}

inline HWND CreateDummyWindow() {
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

    ShowWindow(hWnd, SW_HIDE);
    return hWnd;
}