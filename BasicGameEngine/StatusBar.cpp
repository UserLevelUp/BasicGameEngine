// StatusBar.cpp

#include "StatusBar.h"
#include "resource.h" // or "globals.h" for IDC_STATUSBAR

#define IDC_STATUSBAR 101 // Unique identifier for the status bar

StatusBar::StatusBar()
    : hStatusBar(NULL)
{
}

StatusBar::~StatusBar()
{
}

void StatusBar::Create(HWND hWndParent, HINSTANCE hInstance)
{
    // Create the status bar as a child of the main window
    hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
        hWndParent, (HMENU)IDC_STATUSBAR, hInstance, NULL);

    if (hStatusBar)
    {
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
    }
}

void StatusBar::UpdateText(const wchar_t* text)
{
    if (hStatusBar)
    {
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)text);
    }
}

void StatusBar::Resize()
{
    if (hStatusBar)
    {
        // Resize the status bar when the parent window is resized
        SendMessage(hStatusBar, WM_SIZE, 0, 0);
    }
}
