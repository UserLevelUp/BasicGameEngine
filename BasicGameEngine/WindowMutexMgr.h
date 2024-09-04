#pragma once
#include <windows.h>
#include <string>
#include <unordered_map>

class WindowMutexMgr {
public:
    // Creates a mutex for a new instance and returns a unique name for it
    std::wstring CreateInstanceMutex();

    // Releases a mutex with the given name
    void ReleaseInstanceMutex(const std::wstring& name);

    // Returns the count of active instances
    int GetInstanceCount() const;

    // Destructor to clean up all mutexes
    ~WindowMutexMgr();

private:
    std::unordered_map<std::wstring, HANDLE> instanceMutexes; // Map to store mutex names and handles
    int instanceCount = 0; // Track the number of active instances
};
