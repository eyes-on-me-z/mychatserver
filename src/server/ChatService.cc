#include "ChatService.hpp"
#include "Public.hpp"

#include <mymuduo/Timestamp.h>
#include <mymuduo/Logging.h>
#include <iostream>

using namespace std::placeholders;

// 获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService chatService;
    return &chatService;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap[LOGIN_MSG] = std::bind(
        &ChatService::login, this, _1, _2, _3
    );
    _msgHandlerMap[LOGOUT_MSG] = std::bind(
        &ChatService::logout, this, _1, _2, _3
    );
    _msgHandlerMap[REG_MSG] = std::bind(
        &ChatService::reg, this, _1, _2, _3
    );
    _msgHandlerMap[ONE_CHAT_MSG] = std::bind(
        &ChatService::oneChat, this, _1, _2, _3
    );
    _msgHandlerMap[ADD_FRIEND_MSG] = std::bind(
        &ChatService::addFriend, this, _1, _2, _3
    );

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap[CREATE_GROUP_MSG] = std::bind(
        &ChatService::createGroup, this, _1 ,_2, _3
    );
    _msgHandlerMap[ADD_GROUP_MSG] = std::bind(
        &ChatService::addGroup, this, _1, _2, _3
    );
    _msgHandlerMap[GROUP_CHAT_MSG] = std::bind(
        &ChatService::groupChat, this, _1, _2, _3
    );
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

// 处理登录业务  id pwd 
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp)
{
    int id = js["id"].get<int>();
    std::string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd && id != -1)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _userConnMap[id] = conn;
            }

            // 登录成功，更新用户状态信息 state offline=>online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = id;
            response["name"] = user.getName();

            // 查询该用户是否有离线消息
            std::vector<std::string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            // 查询该用户的好友信息并返回
            std::vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                std::vector<std::string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

            // 查询用户的群组信息
            std::vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                std::vector<std::string> groupV;
                for (Group &group : groupuserVec)
                {
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["groupname"] = group.getName();
                    groupjs["groupdesc"] = group.getDesc();
                    
                    std::vector<std::string> userV;
                    for (GroupUser &groupuser : group.getUsers())
                    {
                        json groupuserjs;
                        groupuserjs["id"] = groupuser.getId();
                        groupuserjs["name"] = groupuser.getName();
                        groupuserjs["state"] = groupuser.getState();
                        groupuserjs["role"] = groupuser.getRole();
                        userV.push_back(groupuserjs.dump());
                    }

                    groupjs["users"] = userV;
                    groupV.push_back(groupjs.dump());
                }
                response["groups"] = groupV;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        // 该用户不存在，用户存在但是密码错误，登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }
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

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr&, json &js, Timestamp)
{
    int toid = js["toid"].get<int>();

    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线，转发消息   服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    // toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务
void ChatService::addFriend(const TcpConnectionPtr&, json &js, Timestamp)
{
    int userId = js["id"].get<int>();
    int friendId = js["friendid"].get<int>();
    _friendModel.insert(userId, friendId);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr&, json &js, Timestamp)
{
    int userId = js["id"].get<int>();
    std::string groupName = js["groupname"];
    std::string groupDesc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, groupName, groupDesc);
    if (_groupModel.createGroup(group))
    {
        _groupModel.addGroup(userId, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr&, json &js, Timestamp)
{
    int userId = js["id"].get<int>();
    int groupId = js["groupid"].get<int>();

    _groupModel.addGroup(userId, groupId, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr&, json &js, Timestamp)
{
    int userId = js["id"].get<int>();
    int groupId = js["groupid"].get<int>();

    std::lock_guard<std::mutex> lock(_mutex);
    for (auto id : _groupModel.queryGroupUsers(userId, groupId))
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            it->second->send(js.dump());
        }
        else
        {
            // 存储离线群消息
            _offlineMsgModel.insert(id, js.dump());
        }
    }
}

// 处理注销业务
void ChatService::logout(const TcpConnectionPtr&, json &js, Timestamp)
{
    int userId = js["id"].get<int>();
    {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _userConnMap.find(userId);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 更新用户的状态信息 online => offline
    User user(userId, "", "", "offline");
    _userModel.updateState(user);
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                // 从map表删除用户的链接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把online状态的用户，设置成offline
    _userModel.resetState();
}