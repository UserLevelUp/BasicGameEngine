#include "WindowMutexMgr.h"
#include <sstream>

std::wstring WindowMutexMgr::CreateInstanceMutex() {
    // Generate a unique name for the new instance mutex
    std::wstringstream mutexNameStream;
    mutexNameStream << L"Global\\BasicGameEngineInstanceMutex_" << instanceCount;

    std::wstring mutexName = mutexNameStream.str();

    // Create a new mutex for this instance
    HANDLE hMutex = ::CreateMutex(NULL, FALSE, mutexName.c_str());
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        return L""; // Mutex creation failed or already exists
    }

    // Store the mutex handle in the map
    instanceMutexes[mutexName] = hMutex;
    instanceCount++; // Increment instance count
    return mutexName;
}

void WindowMutexMgr::ReleaseInstanceMutex(const std::wstring& name) {
    auto it = instanceMutexes.find(name);
    if (it != instanceMutexes.end()) {
        ::ReleaseMutex(it->second);
        ::CloseHandle(it->second); // Close the handle to free resources
        instanceMutexes.erase(it); // Remove the mutex from the map
        instanceCount--; // Decrement instance count
    }
}

int WindowMutexMgr::GetInstanceCount() const {
    return instanceCount;
}

WindowMutexMgr::~WindowMutexMgr() {
    // Release all mutexes when the manager is destroyed
    for (auto& entry : instanceMutexes) {
        ::ReleaseMutex(entry.second);
        ::CloseHandle(entry.second);
    }
    instanceMutexes.clear();
}
