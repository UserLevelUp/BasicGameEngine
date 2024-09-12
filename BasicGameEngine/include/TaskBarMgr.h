#pragma once

#include <windows.h>

class TaskBarMgr
{
public:
    TaskBarMgr();
    ~TaskBarMgr();

    void Initialize(HWND hWnd, HINSTANCE hInstance);
    void AddTrayIcon();   // Adds the tray icon
    void RemoveTrayIcon(); // Removes the tray icon
    void CreateTrayMenu(); // Creates the context menu for the tray icon
    void ShowContextMenu(); // Displays the context menu

private:
    HWND hWnd_;                  // Handle to the main window
    HINSTANCE hInstance_;        // Instance handle
    NOTIFYICONDATA nid_;         // Tray icon data
    HMENU hTrayMenu_;            // Handle to the tray menu
};
