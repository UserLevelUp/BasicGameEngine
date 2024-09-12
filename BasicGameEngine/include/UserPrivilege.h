#pragma once
#include <string>

class UserPrivilege {
public:
    UserPrivilege();
    ~UserPrivilege() = default;

    void SetPrivilegeStatus(const std::wstring& status);
    std::wstring GetPrivilegeStatus() const;

private:
    std::wstring privilegeStatus; // Holds the privilege status (e.g., "Administrator" or "Standard User")
};
