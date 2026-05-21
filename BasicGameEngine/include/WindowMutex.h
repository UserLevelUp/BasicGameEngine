#pragma once

#include <string>
#include <Windows.h>

class WindowMutex {
public:
    // Default constructor
    WindowMutex();

    // Constructor to initialize a mutex with a name
    WindowMutex(const std::wstring& name);

    WindowMutex(const WindowMutex&) = delete;
    WindowMutex& operator=(const WindowMutex&) = delete;
    WindowMutex(WindowMutex&& other) noexcept;
    WindowMutex& operator=(WindowMutex&& other) noexcept;

    // Destructor to release and close the mutex
    ~WindowMutex();

    // Attempt to create or open the mutex
    bool Create();

    // Release the mutex
    void Release();

    // Get the mutex handle
    HANDLE GetHandle() const;

    // Get the mutex name
    std::wstring GetName() const;

private:
    std::wstring name; // Mutex name
    HANDLE handle;     // Mutex handle
};
