#include "WindowMutexMgr.h"
#include <sstream>
#include <objbase.h>  // Required for CoCreateGuid

std::wstring WindowMutexMgr::GenerateGUID() {
    GUID guid;
    CoCreateGuid(&guid);  // Generate a GUID

    // Buffer size is increased to 128 to accommodate full GUID string with prefix
    wchar_t buffer[128];

    // Use swprintf_s with the correct format and size
    swprintf_s(buffer, sizeof(buffer) / sizeof(wchar_t),
        L"Global\\BasicGameEngineInstanceMutex_%08lX-%04X-%04X-%04X-%04X%04X%04X",
        guid.Data1, guid.Data2, guid.Data3,
        (guid.Data4[0] << 8) | guid.Data4[1],
        (guid.Data4[2] << 8) | guid.Data4[3],
        (guid.Data4[4] << 8) | guid.Data4[5],
        (guid.Data4[6] << 8) | guid.Data4[7]);

    return std::wstring(buffer);
}


std::wstring WindowMutexMgr::CreateInstanceMutex() {
    std::wstring mutexName = GenerateGUID();  // Generate a unique name for the mutex

    // Create a new mutex for this instance
    HANDLE hMutex = ::CreateMutex(NULL, FALSE, mutexName.c_str());
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        return L"";  // Mutex creation failed or already exists
    }

    // Store the mutex handle in the map
    instanceMutexes[mutexName] = hMutex;
    instanceCount++;  // Increment instance count
    return mutexName;
}

void WindowMutexMgr::ReleaseInstanceMutex(const std::wstring& name) {
    auto it = instanceMutexes.find(name);
    if (it != instanceMutexes.end()) {
        ::ReleaseMutex(it->second);
        ::CloseHandle(it->second);  // Close the handle to free resources
        instanceMutexes.erase(it);  // Remove the mutex from the map
        instanceCount--;  // Decrement instance count
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
