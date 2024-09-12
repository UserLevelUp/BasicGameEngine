#include "../include/WindowMutex.h"

// Default constructor
WindowMutex::WindowMutex() : handle(nullptr) {}

// Constructor with a name parameter
WindowMutex::WindowMutex(const std::wstring& name) : name(name), handle(nullptr) {}

WindowMutex::~WindowMutex() {
    if (handle) {
        Release();
        CloseHandle(handle); // Ensure the handle is closed
    }
}

bool WindowMutex::Create() {
    if (name.empty()) {
        return false; // If the name is not set, fail to create the mutex
    }

    // Attempt to create or open the mutex
    handle = ::CreateMutex(NULL, FALSE, name.c_str());
    return handle != NULL && GetLastError() != ERROR_ALREADY_EXISTS;
}

void WindowMutex::Release() {
    if (handle) {
        ::ReleaseMutex(handle);
    }
}

HANDLE WindowMutex::GetHandle() const {
    return handle;
}

std::wstring WindowMutex::GetName() const {
    return name;
}
