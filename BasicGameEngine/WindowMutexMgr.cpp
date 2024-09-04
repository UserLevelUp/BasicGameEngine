#include "WindowMutexMgr.h"
#include <sstream>

std::wstring WindowMutexMgr::CreateInstanceMutex() {
    // Generate a unique name for the new instance mutex
    std::wstringstream mutexNameStream;
    mutexNameStream << L"Global\\BasicGameEngineInstanceMutex_" << instanceCount;

    std::wstring mutexName = mutexNameStream.str();

    // Create a new WindowMutex object
    WindowMutex mutex(mutexName);

    if (!mutex.Create()) {
        return L""; // Mutex creation failed or already exists
    }

    // Store the mutex object in the map
    instanceMutexes[mutexName] = std::move(mutex);
    instanceCount++; // Increment instance count
    return mutexName;
}

void WindowMutexMgr::ReleaseInstanceMutex(const std::wstring& name) {
    auto it = instanceMutexes.find(name);
    if (it != instanceMutexes.end()) {
        it->second.Release(); // Release the mutex using the WindowMutex method
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
        entry.second.Release();
    }
    instanceMutexes.clear();
}
