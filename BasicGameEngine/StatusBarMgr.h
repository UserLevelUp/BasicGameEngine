#pragma once
#include <windows.h>
#include <chrono>
#include "StatusBar.h"

class StatusBarMgr
{
public:
    StatusBarMgr();
    ~StatusBarMgr();

    void Create(HWND hWndParent, HINSTANCE hInstance); // Create the status bar
    void Update();                                     // Update the status bar display
    void CalculateFramerate();                         // Calculate the current framerate
    void Resize();                                     // Resize the status bar
    void UpdateInstanceCount(int count);               // New method to update instance count
    void UpdatePrivilegeStatus(const std::wstring& status);

    HWND GetHandle() const;
    void SetStatusText(const std::wstring& text); // Set the text of the status bar
    std::wstring GetStatusText() const; // Get the current text of the status bar

private:
    HWND hStatusBar;                                   // Handle to the status bar
    StatusBar statusBar;                               // Instance of StatusBar to hold data
    std::chrono::steady_clock::time_point lastFrameTime; // Last time a frame was rendered
    std::wstring currentText; // Store the current text of the status bar

    void UpdateText(const wchar_t* text);               // Update the text displayed on the status bar
    int instanceCount; // Store the instance count
    std::wstring privilegeStatus; // Store the privilege status
};
