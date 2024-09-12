#pragma once
#include <windows.h>
#include <string>
#include "SharedMemoryData.h" // Include the shared struct

class InterprocessComm {
public:
    InterprocessComm(const std::wstring& name, size_t size);
    ~InterprocessComm();

    bool CreateSharedMemory();
    void ReleaseSharedMemory();
    SharedMemoryData* GetSharedMemoryPointer() const; // Return a pointer to the shared data

private:
    std::wstring name;
    size_t size;
    HANDLE hMapFile;
    void* pSharedMemory;
};
