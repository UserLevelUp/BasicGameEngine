#pragma once
#include "InterprocessComm.h"
#include <map>
#include <string>

class InterprocessCommMgr
{
public:
    static InterprocessCommMgr& GetInstance();

    InterprocessCommMgr(const InterprocessCommMgr&) = delete; // Disable copy constructor
    InterprocessCommMgr& operator=(const InterprocessCommMgr&) = delete; // Disable assignment operator

    InterprocessComm* CreateInterprocessComm(const std::wstring& name, size_t size);
    void ReleaseInterprocessComm(const std::wstring& name);
    int GetInstanceCount() const;

private:
    InterprocessCommMgr();
    ~InterprocessCommMgr();

    std::map<std::wstring, InterprocessComm*> interprocessComms;
    int instanceCount;
};
