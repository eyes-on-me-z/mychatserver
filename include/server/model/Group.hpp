#pragma once

#include <string>
#include <vector>

#include "GroupUser.hpp"

// Group表的ORM类
class Group
{
public:
    Group(int id = -1, const std::string &name = "", const std::string &desc = "")
        : _id(id), _name(name), _desc(desc), _users()
    {}

    void setId(int id) { _id = id; }
    void setName(const std::string &name) { _name = name; }
    void setDesc(const std::string &desc) { _desc = desc; }

    int getId() const { return _id; }
    std::string getName() const { return _name; }
    std::string getDesc() const { return _desc; }

    // 往_users中插入群成员，我觉得应该写个插入群成员函数
    std::vector<GroupUser> &getUsers() { return _users; }

    const std::vector<GroupUser>& getUsers() const { return _users; }

private:
    int _id;                        // 群id
    std::string _name;              // 群名字
    std::string _desc;              // 群描述
    std::vector<GroupUser> _users;  // 群成员
};