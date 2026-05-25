#pragma once

#include <mymuduo/TcpConnection.h>
#include <functional>
#include <unordered_map>
#include <mutex>

#include "json.hpp"
#include "UserModel.hpp"
#include "FriendModel.hpp"
#include "GroupModel.hpp"
#include "OfflineMsgModel.hpp"

using json = nlohmann::json;
using MsgHandler = std::function<void(const TcpConnectionPtr&, json&, Timestamp)>;

// 聊天服务器业务类
class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService* instance();

    // 获取消息对应的处理器
    MsgHandler getHandler(int msgId);
    
    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr&);

private:
    ChatService();

    // 处理登录业务
    void login(const TcpConnectionPtr&, json&, Timestamp);
    
    // 处理注册业务
    void reg(const TcpConnectionPtr&, json&, Timestamp);

    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr&, json&, Timestamp);

    // 添加好友业务
    void addFriend(const TcpConnectionPtr&, json&, Timestamp);

    // 创建群组业务
    void createGroup(const TcpConnectionPtr&, json&, Timestamp);

    // 加入群组业务
    void addGroup(const TcpConnectionPtr&, json&, Timestamp);

    // 群组聊天业务
    void groupChat(const TcpConnectionPtr&, json&, Timestamp);

    // 处理注销业务
    void logout(const TcpConnectionPtr&, json&, Timestamp);

    // 服务器异常，业务重置方法
    void reset();

    // 存储消息id和其对应的业务处理方法
    std::unordered_map<int, MsgHandler> _msgHandlerMap;
    // 存储在线用户的通信连接
    std::unordered_map<int, TcpConnectionPtr> _userConnMap;

    // 定义互斥锁，保证_userConnMap的线程安全
    std::mutex _mutex;

    // 数据操作类对象
    UserModel _userModel;
    FriendModel _friendModel;
    GroupModel _groupModel;
    OfflineMsgModel _offlineMsgModel;
};