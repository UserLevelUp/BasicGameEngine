#include "../include/TaskBarMgr.h"
#include "../resource.h" // Include your specific resource header file
#include "../include/StatusBarMgr.h"

TaskBarMgr::TaskBarMgr()
    : hWnd_(nullptr), hInstance_(nullptr), hTrayMenu_(nullptr)
{
    ZeroMemory(&nid_, sizeof(nid_));
}

TaskBarMgr::~TaskBarMgr()
{
    RemoveTrayIcon(); // Ensure the tray icon is removed on destruction
    if (hTrayMenu_)
    {
        DestroyMenu(hTrayMenu_);
    }
}

void TaskBarMgr::Initialize(HWND hWnd, HINSTANCE hInstance)
{
    hWnd_ = hWnd;
    hInstance_ = hInstance;
    CreateTrayMenu(); // Initialize the tray menu
}

void TaskBarMgr::AddTrayIcon()
{
    nid_.cbSize = sizeof(NOTIFYICONDATA);
    nid_.hWnd = hWnd_;
    nid_.uID = 1001; // Unique ID for the icon
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_APP; // Custom message identifier
    nid_.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Use an appropriate icon
    lstrcpy(nid_.szTip, L"Basic Game Engine");

    // Add the icon to the system tray
    Shell_NotifyIcon(NIM_ADD, &nid_);
}

void TaskBarMgr::RemoveTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &nid_);
}

void TaskBarMgr::CreateTrayMenu()
{
    hTrayMenu_ = CreatePopupMenu(); // Create a new pop-up menu
    AppendMenu(hTrayMenu_, MF_STRING, ID_TRAY_RESTORE, L"Restore"); // Add "Restore" option
    AppendMenu(hTrayMenu_, MF_STRING, ID_TRAY_EXIT, L"Exit"); // Add "Exit" option
}

void TaskBarMgr::ShowContextMenu()
{
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd_); // Required to show menu properly
    TrackPopupMenu(hTrayMenu_, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd_, NULL);
}
