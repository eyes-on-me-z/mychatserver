#include "OfflineMsgModel.hpp"
#include "Database.hpp"

// 存储用户的离线消息
void OfflineMsgModel::insert(int userId, const std::string &msg)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage values (%d, '%s')", userId, msg.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 删除用户的离线消息
void OfflineMsgModel::remove(int userId)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid = %d", userId);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户的离线消息
std::vector<std::string> OfflineMsgModel::query(int userId)
{
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d", userId);

    std::vector<std::string> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr)
            {
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }

    return vec;
}