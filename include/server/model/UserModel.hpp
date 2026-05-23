#pragma once

#include "User.hpp"

// User表的数据操作类，用来操作用户类
class UserModel
{
public:
    // User表的增加方法
    bool insert(User &user);

    // 查询用户
    User query(int id);

    // 更新用户的状态信息
    bool updateState(User user);

    // 重置用户的状态信息
    void resetState();
};