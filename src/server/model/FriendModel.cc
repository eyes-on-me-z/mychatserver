#include "FriendModel.hpp"
#include "Database.hpp"

// 添加好友关系
void FriendModel::insert(int userId, int friendId)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values (%d, %d)", userId, friendId);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 返回用户好友列表
std::vector<User> FriendModel::query(int userId)
{
    char sql[1024] = {0};
    // inner join表示两张表联表查询，总体表示查询某个用户的好友信息
    sprintf(sql,
        "select a.id, a.name, a.state from users a inner join friend b on b.friendid = a.id where b.userid = %d",
        userId);
    std::vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有好友放入vec中返回
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(std::move(user));
            }
            mysql_free_result(res);
            return vec;
        }
    }

    return vec;
}