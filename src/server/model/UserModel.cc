#include "UserModel.hpp"
#include "DataLib.hpp"

#include <iostream>

// User表的增加方法
bool UserModel::insert(User &user)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into users(name, password, state) values('%s', '%s', '%s')",
            user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if(mysql.update(sql))
        {
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    
    return false;
}

// 查询用户
User UserModel::query(int id)
{
    User user;
    return user;
}

// 更新用户的状态信息
bool UserModel::updateState(User user)
{
    return true;
}

// 重置用户的状态信息
void UserModel::resetState()
{

}