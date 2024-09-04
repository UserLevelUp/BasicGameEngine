#include "InterprocessComm.h"

InterprocessComm::InterprocessComm(const std::wstring& name, size_t size)
    : name(name), size(size), hMapFile(NULL), pSharedMemory(NULL) {}

InterprocessComm::~InterprocessComm() {
    ReleaseSharedMemory();
}

bool InterprocessComm::CreateSharedMemory() {
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

SharedMemoryData* InterprocessComm::GetSharedMemoryPointer() const {
    return reinterpret_cast<SharedMemoryData*>(pSharedMemory); // Return the pointer to the shared memory as SharedMemoryData
}
