#pragma once
#include <wtypes.h>

struct SharedMemoryData {
    LONG instanceCount; // Number of instances running
    // Add any other shared data fields here
    // e.g., bool isPaused;
};