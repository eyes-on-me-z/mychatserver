#pragma once

#include <mymuduo/TcpConnection.h>
#include <functional>
#include <unordered_map>

#include "json.hpp"
#include "UserModel.hpp"

using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr&, json&, Timestamp)>;

class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService* instance();

    // 获取消息对应的处理器
    MsgHandler getHandler(int msgId);

private:
    ChatService();

    // 处理登录业务
    void login(const TcpConnectionPtr&, json&, Timestamp);
    
    // 处理注册业务
    void reg(const TcpConnectionPtr&, json&, Timestamp);

    // 存储消息id和其对应的业务处理方法
    std::unordered_map<int, MsgHandler> _msgHandlerMap;

    UserModel _userModel;
};