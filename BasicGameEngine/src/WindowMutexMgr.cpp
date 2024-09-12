#include "../include/WindowMutexMgr.h"
#include <sstream>
#include <random>  // Include for generating a random number or GUID
#include "../include/SharedMemoryData.h"
#include <iostream>  // Include for debug output
#include "../include/InterprocessCommMgr.h"
#include <fstream>    // Include for file operations

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

    // Generate a GUID for the new instance mutex
    GUID guid;
    HRESULT hr = CoCreateGuid(&guid);
    if (FAILED(hr)) {
        std::wcout << L"Failed to create GUID for mutex." << std::endl; // Debug output
        return L""; // GUID creation failed
    }

    // Convert GUID to a wide string
    wchar_t guidString[85]; // Make sure the buffer is large enough
    swprintf_s(guidString, 85, L"Global\\BasicGameEngineInstanceMutex_%08lX-%04X-%04X-%04X-%04X%08lX",
        guid.Data1, guid.Data2, guid.Data3,
        (guid.Data4[0] << 8) | guid.Data4[1],  // Combine two bytes for a 4-digit part
        (guid.Data4[2] << 8) | guid.Data4[3],  // Combine another two bytes for the next 4-digit part
        ((unsigned long)guid.Data4[4] << 24) | ((unsigned long)guid.Data4[5] << 16) | ((unsigned long)guid.Data4[6] << 8) | guid.Data4[7]  // Combine the last 4 bytes for an 8-digit part
    );

    std::wstring mutexName = guidString;

    // Output the generated mutex name to standard output
    std::wcout << L"Generated Mutex Name: " << mutexName << std::endl;

    // Append the generated mutex name to a log file
    std::wofstream logFile(L"BasicGameEngine.log", std::ios::app); // Open the log file in append mode
    if (logFile.is_open()) {
        logFile << L"Mutex Name: " << mutexName << std::endl; // Write the label and the name to the file
        logFile.close(); // Close the file after writing
    }
    else {
        std::wcout << L"Failed to open log file for writing." << std::endl; // Debug output in case of failure
    }

    // Create a new WindowMutex object
    WindowMutex mutex(mutexName);

    //// Generate a unique name for the new instance mutex using a random number or GUID
    //std::wstringstream mutexNameStream;
    //mutexNameStream << L"Global\\BasicGameEngineInstanceMutex_" << GetCurrentProcessId();

    //std::wstring mutexName = mutexNameStream.str();

    // Create a new WindowMutex object
    //WindowMutex mutex(mutexName);

    if (!mutex.Create()) {
        std::wcout << L"Mutex creation failed: " << mutexName << std::endl; // Debug output
        return L""; // Mutex creation failed or already exists
    }

    // Store the mutex object in the map
    instanceMutexes[mutexName] = std::move(mutex);

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
