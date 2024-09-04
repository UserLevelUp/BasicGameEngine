// BasicGameEngine.cpp : Defines the entry point for the application.

#include "StatusBarMgr.h"
#include "framework.h"
#include "BasicGameEngine.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <mutex> // For std::mutex and std::lock_guard
#include "resource.h"
#include <shellapi.h>
#include "TaskBarMgr.h"
#include "WindowMutexMgr.h"

WindowMutexMgr windowMutexMgr; // Instance of the WindowMutexMgr
TaskBarMgr taskBarMgr; // Instance of the TaskBarMgr class
StatusBarMgr statusBarMgr; // Instance of the StatusBarMgr class

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

std::thread gameThread;        // Game loop thread
std::mutex gameLoopMutex;      // Mutex for synchronizing game loop
bool isFocused = true;         // Indicates whether this instance is focused
bool shouldRun = true;         // Controls the main game loop
bool isPaused = false;         // Indicates whether the game is paused

double lineX = 10.0; // Initial X position of the line
double lineY = 10.0; // Initial Y position of the line
bool movingRight = true;       // Direction of movement
double speed = 0.1;            // Speed in pixels per millisecond (adjustable)

HWND g_hWnd; // Global handle to the main window

// Function declarations
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void GameLoop();               // Game loop function

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Create a unique mutex for this instance using WindowMutexMgr
    std::wstring mutexName = windowMutexMgr.CreateInstanceMutex();
    if (mutexName.empty())  // Check if mutex creation failed
    {
        MessageBox(NULL, L"Another instance is already running.", L"Basic Game Engine", MB_OK | MB_ICONWARNING);
        return 0; // Exit if a mutex could not be created
    }

    // Initialize global strings, register class, etc.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BASICGAMEENGINE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    // Start the game loop in a separate thread
    gameThread = std::thread(GameLoop);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BASICGAMEENGINE));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Clean up when exiting
    shouldRun = false; // Signal the thread to stop
    if (gameThread.joinable())
    {
        gameThread.join(); // Wait for the game thread to finish
    }

    windowMutexMgr.ReleaseInstanceMutex(mutexName); // Release the unique mutex

    return (int)msg.wParam;
}

// Game loop function running in a separate thread
void GameLoop()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER previousTime, currentTime;
    QueryPerformanceCounter(&previousTime);

    const int targetFPS = 60; // Target frames per second
    const double targetFrameDuration = 1000.0 / targetFPS; // Target duration for each frame in milliseconds

    while (shouldRun)
    {
        // Check if this instance should run based on focus and foreground window
        if (!isFocused || GetForegroundWindow() != g_hWnd)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Lock the game loop for thread safety
        std::lock_guard<std::mutex> lock(gameLoopMutex);

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

        // Redraw only the specific area where the line is moving
        RECT updateRect;
        updateRect.left = 0; // Start from the top-left corner of the window
        updateRect.top = 0;
        updateRect.right = 220; // Width of the line drawing area
        updateRect.bottom = 220; // Height of the line drawing area

        InvalidateRect(g_hWnd, &updateRect, FALSE); // Use g_hWnd instead of GetActiveWindow()

        // Update the status bar
        statusBarMgr.Update();

        // Measure time taken for this frame
        QueryPerformanceCounter(&currentTime);
        double frameTime = (double)(currentTime.QuadPart - previousTime.QuadPart) * 1000.0 / frequency.QuadPart;

        // Introduce a more precise delay to cap the frame rate
        if (frameTime < targetFrameDuration)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(targetFrameDuration - frameTime - 1)));

            while (true)
            {
                QueryPerformanceCounter(&currentTime);
                frameTime = (double)(currentTime.QuadPart - previousTime.QuadPart) * 1000.0 / frequency.QuadPart;
                if (frameTime >= targetFrameDuration)
                    break;
            }
        }
    }
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

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BASICGAMEENGINE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_BASICGAMEENGINE);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
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

    g_hWnd = hWnd; // Store the handle in a global variable

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Initialize the taskbar manager
    taskBarMgr.Initialize(hWnd, hInstance);

    // Create the status bar
    statusBarMgr.Create(hWnd, hInstance);

    return TRUE;
}

// Window Procedure to handle window events
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        isFocused = (wParam != WA_INACTIVE);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Double buffering to prevent flickering
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        HBRUSH hbrBkGnd = CreateSolidBrush(RGB(255, 255, 255)); // White background
        FillRect(hdcMem, &ps.rcPaint, hbrBkGnd);
        DeleteObject(hbrBkGnd);

        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0)); // Red line, 2 pixels wide
        HPEN hOldPen = (HPEN)SelectObject(hdcMem, hPen);

        MoveToEx(hdcMem, static_cast<int>(lineX), static_cast<int>(lineY), NULL);
        LineTo(hdcMem, 200, 200);

        SelectObject(hdcMem, hOldPen);
        DeleteObject(hPen);

        BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top, hdcMem, 0, 0, SRCCOPY);

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
            taskBarMgr.ShowContextMenu(); // Use TaskBarMgr to show the context menu
        }
        else if (lParam == WM_LBUTTONDOWN) // Left-click on the tray icon
        {
            ShowWindow(hWnd, SW_RESTORE);
            taskBarMgr.RemoveTrayIcon();
            isPaused = false;
        }
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case ID_TRAY_RESTORE:
            ShowWindow(hWnd, SW_RESTORE);
            taskBarMgr.RemoveTrayIcon();
            isPaused = false; // Resume the game loop
            break;

        case ID_TRAY_EXIT:
            DestroyWindow(hWnd); // Close the application
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;

    case WM_DESTROY:
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
