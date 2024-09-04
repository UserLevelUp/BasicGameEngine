#pragma once
#include <windows.h>
#include <string>
#include <map>

class WindowMutexMgr {
public:
    std::wstring CreateInstanceMutex();
    void ReleaseInstanceMutex(const std::wstring& name);
    int GetInstanceCount() const;
    ~WindowMutexMgr();

private:
    std::map<std::wstring, HANDLE> instanceMutexes;
    int instanceCount = 0;

    std::wstring GenerateGUID();  // Function to generate a unique GUID
};
