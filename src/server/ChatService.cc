#include "ChatService.hpp"
#include "Public.hpp"
#include "User.hpp"

#include <mymuduo/Timestamp.h>
#include <mymuduo/Logging.h>

using namespace std::placeholders;

// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService chatService;
    return &chatService;
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgId)
{
    auto it = _msgHandlerMap.find(msgId);
    if (it == _msgHandlerMap.end()) // 记录错误日志，msgid没有对应的事件处理回调
    {
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr&, json&, Timestamp){
            LOG_ERROR << "msgId:" << msgId << " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgId];
    }
}

ChatService::ChatService()
{
    _msgHandlerMap[LOGIN_MSG] = std::bind(
        &ChatService::login, this, _1, _2, _3
    );
    _msgHandlerMap[REG_MSG] = std::bind(
        &ChatService::reg, this, _1, _2, _3
    );

}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr&, json &js, Timestamp)
{
    LOG_INFO << "do login";
}

// 处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    std::string name = js["name"];
    std::string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);   // 插入用户的时候会设置用户id，state默认是offline

    json response;
    response["msgid"] = REG_MSG_ACK;
    if (state)  // 注册成功
    {
        response["errno"] = 0;
        response["id"] = user.getId();
    }
    else    // 注册失败
    {
        response["errno"] = 1;
    }
    conn->send(response.dump());
}