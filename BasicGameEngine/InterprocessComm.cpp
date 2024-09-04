#include "InterprocessComm.h"

InterprocessComm::InterprocessComm(const std::wstring& name, size_t size)
    : name(name), size(size), hMapFile(NULL), pSharedMemory(nullptr)
{
}

InterprocessComm::~InterprocessComm()
{
    ReleaseSharedMemory();
}

bool InterprocessComm::CreateSharedMemory()
{
    // Create a named file mapping object
    hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, static_cast<DWORD>(size), name.c_str());

    if (hMapFile == NULL)
    {
        MessageBox(NULL, L"Could not create file mapping object.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    // Map the view of the file into the process's address space
    pSharedMemory = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, size);

    if (pSharedMemory == NULL)
    {
        MessageBox(NULL, L"Could not map view of file.", L"Error", MB_OK | MB_ICONERROR);
        CloseHandle(hMapFile);
        return false;
    }

    // Initialize the shared memory if this is the first instance
    if (GetLastError() != ERROR_ALREADY_EXISTS)
    {
        ZeroMemory(pSharedMemory, size); // Initialize memory to zero
    }

    return true;
}

void InterprocessComm::ReleaseSharedMemory()
{
    if (pSharedMemory)
    {
        UnmapViewOfFile(pSharedMemory);
        pSharedMemory = nullptr;
    }

    if (hMapFile)
    {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }
}

void* InterprocessComm::GetSharedMemoryPointer() const
{
    return pSharedMemory;
}
