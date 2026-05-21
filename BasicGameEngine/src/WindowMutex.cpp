#include "../include/WindowMutex.h"
#include <utility>

// Default constructor
WindowMutex::WindowMutex() : handle(nullptr) {}

// Constructor with a name parameter
WindowMutex::WindowMutex(const std::wstring& name) : name(name), handle(nullptr) {}

WindowMutex::WindowMutex(WindowMutex&& other) noexcept
    : name(std::move(other.name)), handle(other.handle) {
    other.handle = nullptr;
}

WindowMutex& WindowMutex::operator=(WindowMutex&& other) noexcept {
    if (this != &other) {
        Release();
        name = std::move(other.name);
        handle = other.handle;
        other.handle = nullptr;
    }

    return *this;
}

WindowMutex::~WindowMutex() {
    Release();
}

bool WindowMutex::Create() {
    if (name.empty()) {
        return false; // If the name is not set, fail to create the mutex
    }

    Release();

    // Attempt to create or open the mutex
    handle = ::CreateMutex(NULL, FALSE, name.c_str());
    return handle != NULL && GetLastError() != ERROR_ALREADY_EXISTS;
}

void WindowMutex::Release() {
    if (handle) {
        CloseHandle(handle);
        handle = nullptr;
    }
}

HANDLE WindowMutex::GetHandle() const {
    return handle;
}

std::wstring WindowMutex::GetName() const {
    return name;
}
