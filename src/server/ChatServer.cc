#include "ChatServer.hpp"
#include "json.hpp"
#include "ChatService.hpp"

#include <functional>
#include <iostream>

using json = nlohmann::json;
using namespace std::placeholders;

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                        const InetAddress &listenAddr,
                        const std::string &nameArg)
    : _server(loop, listenAddr, nameArg)
    , _loop(loop)            
{
    // 注册链接回调
    _server.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1)
    );

    // 注册消息回调
    _server.setMessageCallback(
        std::bind(&ChatServer::onMessage, this, _1, _2, _3)
    );

    // 设置线程数量
    _server.setThreadNum(4);
}

// 启动服务
void ChatServer::start()
{
    _server.start();
}

// 上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开链接
    if (!conn->connected())
    {
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
{
    std::string msg = buf->retrieveAllAsString();

    std::cout << msg << std::endl;

    json js = json::parse(msg);

    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    msgHandler(conn, js, time);
}