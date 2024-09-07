#include "WindowMutexMgr.h"
#include <sstream>
#include "SharedMemoryData.h"
#include "InterprocessCommMgr.h"

std::wstring WindowMutexMgr::CreateInstanceMutex() {
    // Get the shared memory pointer from InterprocessCommMgr
    InterprocessCommMgr& commMgr = InterprocessCommMgr::GetInstance();
    InterprocessComm* comm = commMgr.CreateInterprocessComm(L"Global\\BasicGameEngineSharedMemory", sizeof(SharedMemoryData));
    if (!comm) {
        return L""; // Failed to create or access shared memory
    }

    SharedMemoryData* sharedData = comm->GetSharedMemoryPointer();
    if (!sharedData) {
        return L""; // Failed to get the shared memory pointer
    }

    // Generate a unique name for the new instance mutex
    std::wstringstream mutexNameStream;
    mutexNameStream << L"Global\\BasicGameEngineInstanceMutex_" << sharedData->instanceCount;

    std::wstring mutexName = mutexNameStream.str();

    // Create a new WindowMutex object
    WindowMutex mutex(mutexName);

    if (!mutex.Create()) {
        return L""; // Mutex creation failed or already exists
    }

    // Store the mutex object in the map
    instanceMutexes[mutexName] = std::move(mutex);

    //// Increment the shared instance count safely
    //InterlockedIncrement(&sharedData->instanceCount);

    return mutexName;
}

void WindowMutexMgr::ReleaseInstanceMutex(const std::wstring& name) {
    // Get the shared memory pointer from InterprocessCommMgr
    InterprocessCommMgr& commMgr = InterprocessCommMgr::GetInstance();
    InterprocessComm* comm = commMgr.CreateInterprocessComm(L"Global\\BasicGameEngineSharedMemory", sizeof(SharedMemoryData));
    if (!comm) {
        return; // Failed to create or access shared memory
    }

    SharedMemoryData* sharedData = comm->GetSharedMemoryPointer();
    if (!sharedData) {
        return; // Failed to get the shared memory pointer
    }

    auto it = instanceMutexes.find(name);
    if (it != instanceMutexes.end()) {
        it->second.Release(); // Release the mutex using the WindowMutex method
        instanceMutexes.erase(it); // Remove the mutex from the map

        // Decrement the shared instance count safely
        //InterlockedDecrement(&sharedData->instanceCount);
    }
}

int WindowMutexMgr::GetInstanceCount() const {
    // Get the shared memory pointer from InterprocessCommMgr
    InterprocessCommMgr& commMgr = InterprocessCommMgr::GetInstance();
    InterprocessComm* comm = commMgr.CreateInterprocessComm(L"Global\\BasicGameEngineSharedMemory", sizeof(SharedMemoryData));
    if (!comm) {
        return 0; // Failed to create or access shared memory
    }

    SharedMemoryData* sharedData = comm->GetSharedMemoryPointer();
    if (!sharedData) {
        return 0; // Failed to get the shared memory pointer
    }

    return sharedData->instanceCount;
}

WindowMutexMgr::~WindowMutexMgr() {
    // Release all mutexes when the manager is destroyed
    for (auto& entry : instanceMutexes) {
        entry.second.Release();
    }
    instanceMutexes.clear();
}
