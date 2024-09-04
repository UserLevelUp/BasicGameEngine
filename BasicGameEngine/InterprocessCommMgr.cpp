#include "InterprocessCommMgr.h"

InterprocessCommMgr& InterprocessCommMgr::GetInstance()
{
    static InterprocessCommMgr instance;
    return instance;
}

InterprocessCommMgr::InterprocessCommMgr() : instanceCount(0) {}

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
    if (interprocessComms.find(name) != interprocessComms.end())
    {
        return interprocessComms[name];
    }

    InterprocessComm* comm = new InterprocessComm(name, size);
    if (comm->CreateSharedMemory())
    {
        interprocessComms[name] = comm;
        instanceCount++;
    }
    else
    {
        delete comm;
        comm = nullptr;
    }

    return comm;
}

void InterprocessCommMgr::ReleaseInterprocessComm(const std::wstring& name)
{
    auto it = interprocessComms.find(name);
    if (it != interprocessComms.end())
    {
        delete it->second;
        interprocessComms.erase(it);
        instanceCount--;
    }
}

int InterprocessCommMgr::GetInstanceCount() const
{
    return instanceCount;
}
