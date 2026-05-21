#pragma once
#include <Windows.h>

constexpr LONG BGE_MAX_WORKERS = 32;
constexpr DWORD BGE_ROLE_NONE = 0;
constexpr DWORD BGE_ROLE_CONTROLLER = 1;
constexpr DWORD BGE_ROLE_WORKER = 2;

struct SharedWorkerData {
    DWORD pid;
    DWORD role;
    DWORD64 heartbeatTick;
    wchar_t name[64];
};

struct SharedMemoryData {
    LONG instanceCount; // Number of instances running
    DWORD controllerPid;
    DWORD64 controllerHeartbeatTick;
    LONG workerCount;
    SharedWorkerData workers[BGE_MAX_WORKERS];
};