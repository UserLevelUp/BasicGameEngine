#include "../include/InterprocessComm.h"
#include <iostream>
#include <utility>

InterprocessComm::InterprocessComm(const std::wstring& name, size_t size)
    : name(name), size(size), hMapFile(NULL), pSharedMemory(NULL), attached(false) {
    if (CreateSharedMemory()) {
        // Increment the instance count only when the shared memory is successfully created
        SharedMemoryData* sharedData = GetSharedMemoryPointer();
        if (sharedData) {
            InterlockedIncrement(&sharedData->instanceCount);
            attached = true;
        }
    }
}

InterprocessComm::InterprocessComm(InterprocessComm&& other) noexcept
    : name(std::move(other.name)),
      size(other.size),
      hMapFile(other.hMapFile),
      pSharedMemory(other.pSharedMemory),
      attached(other.attached) {
    other.size = 0;
    other.hMapFile = NULL;
    other.pSharedMemory = NULL;
    other.attached = false;
}

InterprocessComm& InterprocessComm::operator=(InterprocessComm&& other) noexcept {
    if (this != &other) {
        SharedMemoryData* sharedData = GetSharedMemoryPointer();
        if (attached && sharedData) {
            InterlockedDecrement(&sharedData->instanceCount);
        }
        ReleaseSharedMemory();

        name = std::move(other.name);
        size = other.size;
        hMapFile = other.hMapFile;
        pSharedMemory = other.pSharedMemory;
        attached = other.attached;

        other.size = 0;
        other.hMapFile = NULL;
        other.pSharedMemory = NULL;
        other.attached = false;
    }

    return *this;
}

InterprocessComm::~InterprocessComm() {
    // Decrement the instance count only when the instance is destroyed
    SharedMemoryData* sharedData = GetSharedMemoryPointer();
    if (attached && sharedData) {
        LONG newCount = InterlockedDecrement(&sharedData->instanceCount);
        // Debug output
        std::wcout << L"Decremented instance count to: " << newCount << std::endl;
    }

    attached = false;
    ReleaseSharedMemory();
}

bool InterprocessComm::CreateSharedMemory() {
    if (pSharedMemory != NULL) {
        return true;
    }

    hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,    // Use paging file
        NULL,                    // Default security
        PAGE_READWRITE,          // Read/write access
        0,                       // Maximum object size (high-order DWORD)
        size,                    // Maximum object size (low-order DWORD)
        name.c_str());           // Name of the mapping object

    if (hMapFile == NULL) {
        return false;
    }

    // Check if the memory mapping object already exists or not
    bool alreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);

    pSharedMemory = MapViewOfFile(
        hMapFile,               // Handle to map object
        FILE_MAP_ALL_ACCESS,    // Read/write permission
        0,
        0,
        size);

    if (pSharedMemory == NULL) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
        return false;
    }

    // If this is a new shared memory (not previously existing), initialize it
    if (!alreadyExists) {
        ZeroMemory(pSharedMemory, size); // Initialize the shared memory to zero
    }

    return true;
}

void InterprocessComm::ReleaseSharedMemory() {
    if (pSharedMemory != NULL) {
        UnmapViewOfFile(pSharedMemory);
        pSharedMemory = NULL;
    }
    if (hMapFile != NULL) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }
}

bool InterprocessComm::IsValid() const {
    return pSharedMemory != NULL;
}

SharedMemoryData* InterprocessComm::GetSharedMemoryPointer() const {
    return reinterpret_cast<SharedMemoryData*>(pSharedMemory); // Return the pointer to the shared memory as SharedMemoryData
}
