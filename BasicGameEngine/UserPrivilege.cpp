#include "UserPrivilege.h"

UserPrivilege::UserPrivilege() : privilegeStatus(L"Unknown") {}

void UserPrivilege::SetPrivilegeStatus(const std::wstring& status) {
    privilegeStatus = status;
}

std::wstring UserPrivilege::GetPrivilegeStatus() const {
    return privilegeStatus;
}
