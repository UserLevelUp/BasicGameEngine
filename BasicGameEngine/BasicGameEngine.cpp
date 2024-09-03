// BasicGameEngine.cpp : Defines the entry point for the application.
//

#include "StatusBarMgr.h"

#include "framework.h"
#include "BasicGameEngine.h"
#include <chrono>
#include <iostream>
#include <thread>
#include "resource.h" // or "globals.h"
#include <shellapi.h>

// resource.h - Resource file for BasicGameEngine

NOTIFYICONDATA nid;
HMENU hTrayMenu; // Handle to the tray icon menu
HMENU hMainMenu; // Handle to the main menu

StatusBarMgr statusBarMgr; // Instance of the StatusBarMgr class

// Define global variables
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

double lineX = 10.0; // Use double for precision
double lineY = 10.0; // Use double for precision

bool movingRight = true;         // Direction of movement
double speed = 0.1;              // Speed in pixels per millisecond (adjustable)

bool isRunning = true;     // Controls the main game loop
bool isPaused = false;     // Indicates whether the game is paused

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BASICGAMEENGINE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BASICGAMEENGINE));

    MSG msg;
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER previousTime, currentTime;
    QueryPerformanceCounter(&previousTime);

    const int targetFPS = 60; // Target frames per second
    const double targetFrameDuration = 1000.0 / targetFPS; // Target duration for each frame in milliseconds

    while (isRunning)
    {
        // Process Windows messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                isRunning = false;
                break;
            }

            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        // Check if the window is minimized or paused
        if (IsIconic(GetActiveWindow()) || isPaused)
        {
            // Sleep to reduce CPU usage when minimized or paused
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue; // Skip the rest of the loop
        }

        // Calculate deltaTime in milliseconds
        QueryPerformanceCounter(&currentTime);
        double deltaTime = (double)(currentTime.QuadPart - previousTime.QuadPart) * 1000.0 / frequency.QuadPart;
        previousTime = currentTime; // Update the previous time

        // Update the position of the line based on speed and deltaTime
        if (movingRight)
        {
            lineX += speed * deltaTime; // Adjust the position based on speed and deltaTime
            if (lineX >= 200) movingRight = false; // Stop and reverse direction
        }
        else
        {
            lineX -= speed * deltaTime; // Adjust the position based on speed and deltaTime
            if (lineX <= 10) movingRight = true; // Stop and reverse direction
        }

        // Ensure the line does not exceed its boundaries
        if (lineX < 10) lineX = 10; // Clamp to lower bound
        if (lineX > 200) lineX = 200; // Clamp to upper bound

        // Request a redraw of the window
        InvalidateRect(GetActiveWindow(), NULL, FALSE); // Request a redraw
        UpdateWindow(GetActiveWindow()); // Force an immediate redraw

        // Update the status bar
        statusBarMgr.Update();

        // Measure time taken for this frame
        QueryPerformanceCounter(&currentTime);
        double frameTime = (double)(currentTime.QuadPart - previousTime.QuadPart) * 1000.0 / frequency.QuadPart;

        // Introduce a delay to cap the frame rate
        if (frameTime < targetFrameDuration)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(targetFrameDuration - frameTime)));
        }
    }

    return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BASICGAMEENGINE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_BASICGAMEENGINE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Create the status bar
    statusBarMgr.Create(hWnd, hInstance);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED)
        {
            isPaused = true;  // Pause the game loop if minimized

            // Hide the window and add the system tray icon
            ShowWindow(hWnd, SW_HIDE);
            AddTrayIcon(hWnd);
        }
        else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
        {
            isPaused = false; // Resume the game loop if restored or maximized

            // Remove the tray icon when restored
            RemoveTrayIcon();
        }

        // Handle other size-related actions
        statusBarMgr.Resize();
        InvalidateRect(hWnd, NULL, TRUE);
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Double buffering to prevent flickering
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        // Clear the memory context
        HBRUSH hbrBkGnd = CreateSolidBrush(RGB(255, 255, 255)); // White background
        FillRect(hdcMem, &ps.rcPaint, hbrBkGnd);
        DeleteObject(hbrBkGnd);

        // Set the pen color and draw a line on the memory context
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0)); // Red line, 2 pixels wide
        HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);

        // Draw the line from (lineX, lineY) to (200, 200)
        MoveToEx(hdcMem, static_cast<int>(lineX), static_cast<int>(lineY), NULL);
        LineTo(hdcMem, 200, 200);

        // Restore the old pen and clean up
        SelectObject(hdcMem, hOldPen);
        DeleteObject(hPen);

        // Copy the off-screen buffer to the main window
        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, hdcMem, 0, 0, SRCCOPY);

        // Clean up the memory device context and bitmap
        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        EndPaint(hWnd, &ps);
    }
    break;  

    case WM_APP: // Custom message identifier for tray icon
    {
        if (lParam == WM_RBUTTONDOWN) // Right-click on the tray icon
        {
            // Display the context menu
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hWnd); // Required to show menu properly
            TrackPopupMenu(hTrayMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
        }
        else if (lParam == WM_LBUTTONDOWN) // Left-click on the tray icon
        {
            // Restore the window when left-clicked
            ShowWindow(hWnd, SW_RESTORE);
            RemoveTrayIcon(); // Remove the tray icon
            isPaused = false; // Resume the game loop
        }
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Handle menu commands from the tray menu
        switch (wmId)
        {
        case ID_TRAY_RESTORE: // Restore option
            ShowWindow(hWnd, SW_RESTORE);
            RemoveTrayIcon();
            isPaused = false; // Resume the game loop
            break;

        case ID_TRAY_EXIT: // Exit option
            DestroyWindow(hWnd); // Close the application
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_DESTROY:
        RemoveTrayIcon(); // Clean up tray icon on exit
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {   
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

void AddTrayIcon(HWND hWnd)
{
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1001; // Unique ID for the icon
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP; // Custom message identifier
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Use an appropriate icon
    lstrcpy(nid.szTip, L"Basic Game Engine");

    // Add the icon to the system tray
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void RemoveTrayIcon()
{
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void CreateTrayMenu()
{
    hTrayMenu = CreatePopupMenu(); // Create a new pop-up menu
    AppendMenu(hTrayMenu, MF_STRING, ID_TRAY_RESTORE, L"Restore"); // Add "Restore" option
    AppendMenu(hTrayMenu, MF_STRING, ID_TRAY_EXIT, L"Exit"); // Add "Exit" option
}

