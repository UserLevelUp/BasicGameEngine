#pragma once

#include "WindowMutex.h"
#include <map>
#include <string>

class WindowMutexMgr {
public:
    // Create or open a new instance mutex
    std::wstring CreateInstanceMutex();

    // Release an instance mutex
    void ReleaseInstanceMutex(const std::wstring& name);

    // Get the count of active instances
    int GetInstanceCount() const;

    // Destructor to clean up all mutexes
    ~WindowMutexMgr();

private:
    std::map<std::wstring, WindowMutex> instanceMutexes; // Use WindowMutex objects
    int instanceCount = 0;
};
