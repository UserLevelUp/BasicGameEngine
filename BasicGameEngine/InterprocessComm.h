#pragma once
#include <windows.h>
#include <string>

class InterprocessComm
{
public:
    InterprocessComm(const std::wstring& name, size_t size);
    ~InterprocessComm();

    bool CreateSharedMemory();
    void ReleaseSharedMemory();
    void* GetSharedMemoryPointer() const;

private:
    std::wstring name;
    size_t size;
    HANDLE hMapFile;
    void* pSharedMemory;
};
