#include "DataLib.hpp"
#include <mymuduo/Logging.h>

static std::string server = "127.0.0.1";
static std::string usr = "root";
static std::string password = "123456";
static std::string dbname = "chat";

// 初始化数据库连接
MySQL::MySQL()
{
    // mysql_init函数用于初始化MYSQL对象，返回一个指向MYSQL对象的指针，如果参数为nullptr，则会自动分配一个新的MYSQL对象并返回其指针。
    _conn = mysql_init(nullptr);
}

// 释放数据库连接资源
MySQL::~MySQL()
{
    if (_conn != nullptr)
    {
        // mysql_close函数用于关闭MYSQL连接，释放相关资源。它接受一个指向MYSQL对象的指针作为参数，并关闭该连接。
        mysql_close(_conn);
    }
}

// 连接数据库
bool MySQL::connect()
{
    // mysql_real_connect函数用于建立与MySQL数据库的连接。它接受多个参数，
    // 包括MYSQL对象指针、服务器地址、用户名、密码、数据库名称、端口号等，
    // 并返回一个指向MYSQL对象的指针，如果连接成功则返回该指针，否则返回nullptr。
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), usr.c_str(), password.c_str(),
                                    dbname.c_str(), 3306, nullptr, 0);
    if (p != nullptr)
    {
        // C和C++代码默认的编码字符是ASCII，如果不设置，从MySQL上拉下来的中文显示？
        // 解决方法：在连接数据库成功后，执行一条SQL语句，设置MySQL的字符集为gbk，这样就可以正确显示中文了。
        mysql_query(_conn, "set names gbk");
        LOG_INFO << "connect mysql success!";

        return true;
    }
    else
    {
        LOG_ERROR << "connect mysql fail!";
        return false;
    }
}

// 更新操作
bool MySQL::update(std::string sql)
{
    // mysql_query函数用于执行SQL查询。它接受一个指向MYSQL对象的指针和一个SQL查询字符串作为参数，
    // 并返回一个整数值，表示查询的结果。
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ':' << __LINE__ << ':'
                << sql << "更新失败！";
        return false;
    }

    return true;
}

// 查询操作
MYSQL_RES* MySQL::query(std::string sql)
{
    // mysql_query函数用于执行SQL查询。它接受一个指向MYSQL对象的指针和一个SQL查询字符串作为参数，
    // 并返回一个整数值，表示查询的结果。
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ':' << __LINE__ << ':'
                 << sql << "查询失败！";
        return nullptr;
    }

    // mysql_use_result函数用于获取查询结果。它接受一个指向MYSQL对象的指针作为参数，
    // 并返回一个指向MYSQL_RES结构的指针，
    return mysql_use_result(_conn);
}

// 获取连接
MYSQL* MySQL::getConnection()
{
    return _conn;
}