#include "../include/UserPrivilegeMgr.h"
#include <iostream>

UserPrivilegeMgr::UserPrivilegeMgr() {
    DetectPrivilege(); // Detect privilege upon initialization
}

bool UserPrivilegeMgr::IsRunningAsAdmin() const {
    BOOL isAdmin = FALSE;
    PSID adminGroup;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(
        &ntAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {

        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}

void UserPrivilegeMgr::DetectPrivilege() {
    if (IsRunningAsAdmin()) {
        userPrivilege.SetPrivilegeStatus(L"Administrator");
    }
    else {
        userPrivilege.SetPrivilegeStatus(L"Standard User");
    }
}

UserPrivilege UserPrivilegeMgr::GetUserPrivilege() const {
    return userPrivilege;
}
