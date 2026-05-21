#pragma once
#include <windows.h>
#include <string>
#include "SharedMemoryData.h" // Include the shared struct

class InterprocessComm {
public:
    InterprocessComm(const std::wstring& name, size_t size);
    ~InterprocessComm();

    InterprocessComm(const InterprocessComm&) = delete;
    InterprocessComm& operator=(const InterprocessComm&) = delete;
    InterprocessComm(InterprocessComm&& other) noexcept;
    InterprocessComm& operator=(InterprocessComm&& other) noexcept;

    bool CreateSharedMemory();
    void ReleaseSharedMemory();
    bool IsValid() const;
    SharedMemoryData* GetSharedMemoryPointer() const; // Return a pointer to the shared data

private:
    std::wstring name;
    size_t size;
    HANDLE hMapFile;
    void* pSharedMemory;
    bool attached;
};
