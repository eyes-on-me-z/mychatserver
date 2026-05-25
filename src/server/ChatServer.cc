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
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数（工作在多线程）
void ChatServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
{
    std::string msg = buf->retrieveAllAsString();

    // 测试，添加json打印代码
    std::cout << msg << std::endl;

    json js = json::parse(msg); // 反序列化

    // 达到的目的：完全解耦网络模块的代码和业务模块的代码
    // 通过js["msgid"] 获取=》业务handler=》conn  js  time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    
    // 回调消息绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);
}