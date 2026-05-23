#pragma once

#include "User.hpp"

#include <string>

// 群组用户，多了一个role角色信息，从User类直接继承，复用User的其它信息
class GroupUser : public User
{
public:
    void setRole(const std::string &role) { _role = role; }

    std::string getRole() const { return _role; }

private:
    std::string _role;
};