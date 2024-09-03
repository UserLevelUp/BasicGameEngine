#pragma once
#include <windows.h>
#include <CommCtrl.h>

class StatusBar
{
public:
    StatusBar();
    ~StatusBar();

    void Create(HWND hWndParent, HINSTANCE hInstance); // Create the status bar
    void UpdateText(const wchar_t* text);              // Update the text displayed on the status bar
    void Resize();                                     // Resize the status bar

private:
    HWND hStatusBar; // Handle to the status bar (private to encapsulate it)
};
