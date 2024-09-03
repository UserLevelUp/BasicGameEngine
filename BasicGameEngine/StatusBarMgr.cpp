#include "StatusBarMgr.h"
#include <CommCtrl.h>

StatusBarMgr::StatusBarMgr()
    : hStatusBar(NULL)
{
    // Initialize the last frame time to the current time
    lastFrameTime = std::chrono::steady_clock::now();
}

StatusBarMgr::~StatusBarMgr()
{
}

void StatusBarMgr::Create(HWND hWndParent, HINSTANCE hInstance)
{
    // Create the status bar as a child of the main window
    hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
        hWndParent, (HMENU)101, hInstance, NULL);

    if (hStatusBar)
    {
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)L"FPS: 0.00");
    }
}

void StatusBarMgr::Update()
{
    // Calculate the framerate
    CalculateFramerate();

    // Get the status message and framerate from the StatusBar class
    std::wstring statusText = statusBar.GetStatus();
    double fps = statusBar.GetFramerate();

    // Create a formatted string to display
    wchar_t displayText[100];
    swprintf_s(displayText, 100, L"%s | FPS: %.2f", statusText.c_str(), fps);

    // Update the text displayed on the status bar
    UpdateText(displayText);
}

void StatusBarMgr::CalculateFramerate()
{
    // Get the current time
    auto currentTime = std::chrono::steady_clock::now();

    // Calculate the time difference between the current and last frame
    std::chrono::duration<double, std::milli> elapsed = currentTime - lastFrameTime;

    // Calculate the framerate in frames per second
    if (elapsed.count() > 0)
    {
        double fps = 1000.0 / elapsed.count(); // Convert milliseconds to seconds
        statusBar.SetFramerate(fps); // Update the framerate in the StatusBar class
    }

    // Update the last frame time
    lastFrameTime = currentTime;
}

void StatusBarMgr::Resize()
{
    if (hStatusBar)
    {
        SendMessage(hStatusBar, WM_SIZE, 0, 0); // Resize the status bar to match the parent window's new size
    }
}

void StatusBarMgr::UpdateText(const wchar_t* text)
{
    if (hStatusBar)
    {
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)text);
    }
}
