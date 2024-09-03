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
