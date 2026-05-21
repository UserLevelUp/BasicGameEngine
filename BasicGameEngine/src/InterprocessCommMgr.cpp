#include "../include/InterprocessCommMgr.h"
#include <iostream>

InterprocessCommMgr& InterprocessCommMgr::GetInstance()
{
    static InterprocessCommMgr instance;
    return instance;
}

// Constructor
InterprocessCommMgr::InterprocessCommMgr() : channelCount(0) {}

// Destructor
InterprocessCommMgr::~InterprocessCommMgr()
{
    for (auto& entry : interprocessComms)
    {
        delete entry.second;
    }
    interprocessComms.clear();
}

InterprocessComm* InterprocessCommMgr::CreateInterprocessComm(const std::wstring& name, size_t size)
{
    // Check if an instance with the same name already exists
    if (interprocessComms.find(name) != interprocessComms.end())
    {
        return interprocessComms[name];
    }

    // Create a new InterprocessComm instance
    InterprocessComm* comm = new InterprocessComm(name, size);
    if (comm->IsValid())
    {
        interprocessComms[name] = comm;
        channelCount++;
    }
    else
    {
        delete comm;
        comm = nullptr;
    }

    return comm;
}

// Release an InterprocessComm instance and manage shared memory
void InterprocessCommMgr::ReleaseInterprocessComm(const std::wstring& name) {
    auto it = interprocessComms.find(name);
    if (it != interprocessComms.end()) {
        // Clean up the InterprocessComm instance
        delete it->second;
        interprocessComms.erase(it);
        channelCount--;
    }
}


int InterprocessCommMgr::GetInstanceCount() const
{
    return channelCount;
}
