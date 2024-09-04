#include "StatusBarMgr.h"
#include <CommCtrl.h>

StatusBarMgr::StatusBarMgr()
    : hStatusBar(NULL), instanceCount(0) // Initialize instanceCount
{
    lastFrameTime = std::chrono::steady_clock::now();
}

StatusBarMgr::~StatusBarMgr()
{
}

void StatusBarMgr::Create(HWND hWndParent, HINSTANCE hInstance)
{
    hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
        hWndParent, (HMENU)101, hInstance, NULL);

    if (hStatusBar)
    {
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)L"FPS: 0.00 | Instances: 0");
    }
}

void StatusBarMgr::Update()
{
    // Calculate the framerate
    CalculateFramerate();

    // Get the status message and framerate from the StatusBar class
    std::wstring statusText = statusBar.GetStatus();
    double fps = statusBar.GetFramerate();

    // Create a formatted string to display, including instance count
    wchar_t displayText[100];
    swprintf_s(displayText, 100, L"%s | FPS: %.2f | Instances: %d", statusText.c_str(), fps, instanceCount);

    // Update the text displayed on the status bar
    UpdateText(displayText);
}

void StatusBarMgr::CalculateFramerate()
{
    auto currentTime = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed = currentTime - lastFrameTime;

    if (elapsed.count() > 0)
    {
        double fps = 1000.0 / elapsed.count();
        statusBar.SetFramerate(fps);
    }

    lastFrameTime = currentTime;
}

void StatusBarMgr::Resize()
{
    if (hStatusBar)
    {
        SendMessage(hStatusBar, WM_SIZE, 0, 0);
    }
}

void StatusBarMgr::UpdateText(const wchar_t* text)
{
    if (hStatusBar)
    {
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)text);
    }
}

void StatusBarMgr::UpdateInstanceCount(int count)
{
    instanceCount = count; // Set the new instance count
    Update(); // Refresh the status bar text with the new count
}
