#include "StatusBarMgr.h"
#include <CommCtrl.h>

StatusBarMgr::StatusBarMgr()
    : hStatusBar(NULL), framerate(0.0)
{
    // Initialize the last frame time to the current time
    lastFrameTime = std::chrono::steady_clock::now();
}

StatusBarMgr::~StatusBarMgr()
{
}

void StatusBarMgr::Create(HWND hWndParent, HINSTANCE hInstance)
{
    // Ensure the control ID for the status bar is unique and correctly defined
    const int STATUSBAR_ID = 101; // Unique ID for the status bar

    // Create the status bar as a child of the main window
    hStatusBar = CreateWindowEx(
        0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hWndParent, (HMENU)STATUSBAR_ID, hInstance, NULL);

    if (hStatusBar)
    {
        // Set the initial text of the status bar
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)L"FPS: 0.00");
    }
}

void StatusBarMgr::Update()
{
    // Calculate the framerate
    CalculateFramerate();

    // Convert the framerate to a string and update the status bar text
    wchar_t fpsText[50];
    swprintf_s(fpsText, 50, L"FPS: %.2f", framerate);
    UpdateText(fpsText);
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
        framerate = 1000.0 / elapsed.count(); // Convert milliseconds to seconds
    }

    // Update the last frame time
    lastFrameTime = currentTime;
}

void StatusBarMgr::UpdateText(const wchar_t* text)
{
    if (hStatusBar)
    {
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)text);
    }
}

void StatusBarMgr::Resize()
{
    if (hStatusBar)
    {
        SendMessage(hStatusBar, WM_SIZE, 0, 0); // Resize the status bar to match the parent window's new size
    }
}
