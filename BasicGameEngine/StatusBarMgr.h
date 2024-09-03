#pragma once
#include <windows.h>
#include <chrono>

class StatusBarMgr
{
public:
    StatusBarMgr();
    ~StatusBarMgr();

    void Create(HWND hWndParent, HINSTANCE hInstance); // Create the status bar
    void Update();                                     // Update the status bar text with framerate
    void CalculateFramerate();                         // Calculate the current framerate
    void Resize();                                     // Resize the status bar

private:
    HWND hStatusBar; // Handle to the status bar
    double framerate; // Stores the calculated framerate
    std::chrono::steady_clock::time_point lastFrameTime; // Last time a frame was rendered

    void UpdateText(const wchar_t* text); // Update the text displayed on the status bar
};
