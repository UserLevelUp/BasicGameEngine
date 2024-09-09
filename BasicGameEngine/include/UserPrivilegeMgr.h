#pragma once
#include <windows.h>
#include <string>
#include "UserPrivilege.h"

class UserPrivilegeMgr {
public:
    UserPrivilegeMgr();
    ~UserPrivilegeMgr() = default;

    bool IsRunningAsAdmin() const;
    void DetectPrivilege(); // Detects the privilege level and updates the model
    UserPrivilege GetUserPrivilege() const; // Returns the privilege model

private:
    UserPrivilege userPrivilege; // Instance of UserPrivilege model
};
