#include "User.hpp"
#include "Group.hpp"
#include "json.hpp"
#include "Public.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <semaphore.h>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <unordered_map>

using json = nlohmann::json;

// 记录当前系统登录的用户信息
User g_currentUser;

// 记录当前登录用户的好友列表信息
std::vector<User> g_currentUserFriendList;

// 记录当前登录用户的群组列表信息
std::vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;

// 记录登录状态
std::atomic_bool g_isLoginSuccess{false};

// 接收线程
void readTaskHandler(int clientfd);

// 获取系统时间（聊天信息需要添加时间信息）
std::string getCurrentTime();

// 主聊天页面程序
void mainMenu(int);

// 显示当前登录成功用户的基本信息
void showCurrentUserData();


// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "command invalid example: ./ChatClient 127.0.0.1 8080" << std::endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        std::cerr << "socket create error" << std::endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    struct sockaddr_in server;
    memset(&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(port);

    // 服务端：socket bind listen accept recv/send
    // 客户端：socket connect   recv/send，客户端不需要被动等待别人连它，只需要主动连服务器。
    if (connect(clientfd, (struct sockaddr*)&server, sizeof server) == -1)
    {
        std::cerr << "connect server error" << std::endl;
        close(clientfd);
        exit(-1);
    }

    // 初始化读写线程通信用的信号量
    // pshared=0表示线程间共享（同一进程内线程使用）, 信号量初始值为 0
    sem_init(&rwsem, 0, 0);

    // 连接服务器成功，启动接收子线程
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    // main线程用于接收用户输入，子线程负责发送数据
    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        std::cout << "========================" << std::endl;
        std::cout << "1. login" << std::endl
                  << "2. register" << std::endl
                  << "3. quit" << std::endl;
        std::cout << "========================" << std::endl;
        int choice = 0;
        if (!(std::cin >> choice))  // 处理用户输入非数字情况
        {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cerr << "invalid input!" << std::endl;
            continue;
        }
        std::cin.get();     // 读掉缓冲区残留的回车，避免下一次读取时直接读到回车。

        switch(choice)
        {
        case 1:     // login业务
        {
            int id = -1;
            char pwd[50] = {0};
            std::cout << "userid:";
            std::cin >> id;
            std::cin.get();
            std::cout << "userpassword:";
            std::cin.getline(pwd, sizeof pwd);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            std::string request = js.dump();

            g_isLoginSuccess = false;

            int len = send(clientfd, request.c_str(), request.size(), 0);
            if (len == -1)
            {
                std::cerr << "send login message error: " << request << std::endl;
            }

            sem_wait(&rwsem);

            if (g_isLoginSuccess)
            {
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
            break;
        }
        case 2:     // register业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            std::cout << "username:";
            std::cin.getline(name, sizeof name);
            std::cout << "userpassword:";
            std::cin.getline(pwd, sizeof pwd);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            std::string request = js.dump();

            int len = send(clientfd, request.c_str(), request.size(), 0);
            if (len == -1)
            {
                std::cerr << "send register message error: " << request << std::endl;
            }

            sem_wait(&rwsem);   // 等待信号量，子线程处理完注册消息会通知
            break;
        }
        case 3:     // quit业务
        {
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        }
        default:
            std::cerr << "invalid input!" << std:: endl;
            break;
        }
    }

    return 0;
}

// 处理注册的响应逻辑
void doRegResponse(const json &responseJs)
{
    if (responseJs["errno"] != 0)   // 注册失败
    {
        std::cerr << "name is already exist, register error!" << std::endl;
    }
    else    // 注册成功
    {
        std::cout << "name register success, userid is: " << responseJs["id"]
                    << ", do not forget it!" << std::endl;
    }
}

// 处理登录的响应逻辑
void doLoginResponse(json &responseJs)
{
    if (responseJs["errno"].get<int>() != 0)   // 登录失败
    {
        std::cerr << responseJs["errmsg"] << std::endl;
        g_isLoginSuccess = false;
    }
    else    // 登录成功
    {
        // 记录当前登录用户的id和name
        g_currentUser.setId(responseJs["id"].get<int>());
        g_currentUser.setName(responseJs["name"]);
        g_currentUserFriendList.clear();
        g_currentUserGroupList.clear();

        // 记录当前用户的好友列表信息
        if (responseJs.contains("friends"))
        {
            std::vector<std::string> vec = responseJs["friends"];
            for (std::string &str : vec)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(std::move(user));
            }
        }

        // 记录当前用户的群组列表信息
        if (responseJs.contains("groups"))
        {
            std::vector<std::string> vec1 = responseJs["groups"];
            for (auto &str : vec1)
            {
                json groupJs = json::parse(str);
                Group group;
                group.setId(groupJs["id"].get<int>());
                group.setName(groupJs["groupname"]);
                group.setDesc(groupJs["groupdesc"]);

                std::vector<std::string> vec2 = groupJs["users"];
                for (auto &userStr : vec2)
                {
                    json js = json::parse(userStr);
                    GroupUser user;
                    user.setId(js["id"].get<int>());
                    user.setName(js["name"]);
                    user.setState(js["state"]);
                    user.setRole(js["role"]);
                    group.getUsers().push_back(std::move(user));
                }
                g_currentUserGroupList.push_back(std::move(group));
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息  个人聊天信息或者群组消息
        if (responseJs.contains("offlinemsg"))
        {
            std::vector<std::string> vec = responseJs["offlinemsg"];
            for (auto &str : vec)
            {
                json js = json::parse(str);
                if (js["msgid"].get<int>() == ONE_CHAT_MSG)
                {
                    std::cout << js["time"].get<std::string>() << " [" << js["id"] << "]"
                        << js["name"].get<std::string>() << " said: " << js["msg"].get<std::string>() << std::endl;
                }
                else
                {
                    std::cout << "群消息[" << js["groupid"] << "]: " << js["time"].get<std::string>() << " ["
                        << js["id"] << "]" << js["name"].get<std::string>() << " said: " << js["msg"].get<std::string>() << std::endl;
                }
            }
        }

        g_isLoginSuccess = true;
    }
}

// 子线程 - 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, sizeof buffer, 0); // 阻塞了
        if (len == -1 || len == 0)
        {
            close(clientfd);
            exit(-1);
        }
        std::string strRecv(buffer, len);
        
        // 接收ChatServer转发的数据，反序列化生成json数据对象
        json js;
        try     // 处理json解析异常，防止服务端挂掉
        {
            js = json::parse(strRecv);
        }
        catch (const nlohmann::json::parse_error &e)
        {
            std::cerr << "json parse error: " << e.what()
              << "\nraw: " << buffer << std::endl;
            continue;
        }

        int msgType = js["msgid"].get<int>();
        if (msgType == ONE_CHAT_MSG)
        {
            std::cout << js["time"].get<std::string>() << " [" << js["id"] << "]" << js["name"].get<std::string>()
                << " said: " << js["msg"].get<std::string>() << std::endl;
            continue;
        }
        if (msgType == GROUP_CHAT_MSG)
        {
            std::cout << "群消息[" << js["groupid"] << "]: " << js["time"].get<std::string>() << " ["
                << js["id"] << "]" << js["name"].get<std::string>() << " said: " << js["msg"].get<std::string>() << std::endl;
            continue;
        }
        if (msgType == LOGIN_MSG_ACK)
        {
            doLoginResponse(js);    // 处理登录响应的业务逻辑
            sem_post(&rwsem);       // 通知主线程，登录结果处理完成
            continue;   
        }
        if (msgType == REG_MSG_ACK)
        {
            doRegResponse(js);
            sem_post(&rwsem);       // 通知主线程，注册结果处理完成
            continue;;
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    std::cout << "===============login user=======================" << std::endl;
    std::cout << "current login user => id: " << g_currentUser.getId()
                << ", name: " << g_currentUser.getName() << std::endl;

    std::cout << std:: endl;

    std::cout << "---------------friend list----------------------" << std::endl;
    if (!g_currentUserFriendList.empty())
    {
        for (const auto &user : g_currentUserFriendList)
        {
            std:: cout << user.getId() << " " << user.getName() << " " << user.getState() << std::endl;
        }
    }

    std::cout << std:: endl;

    std::cout << "---------------group list----------------------" << std::endl;
    if (!g_currentUserGroupList.empty())
    {
        for (const auto &group : g_currentUserGroupList)
        {
            std::cout << group.getId() << " " << group.getName() << " " << group.getDesc() << std::endl;
            for (const auto &user : group.getUsers())
            {
                std::cout << user.getId() << " " << user.getName() << " " << user.getState()
                            << " " << user.getRole() << std::endl;
            }
        }
    }

    std::cout << std::endl;

    std::cout << "===============================================" << std::endl;
}

// "help" command handler
void help(int fd = -1, std::string str = "");
// "chat" command handler   str格式为：friendid:message
void chat(int clientfd, std::string str);
// "addfriend" command handler
void addFriend(int clientfd, std::string str);
// "creategroup" command handler  groupname:groupdesc
void createGroup(int clientfd, std::string str);
// "addgroup" command handler
void addGroup(int clientfd, std::string str);
// "groupchat" command handler   groupid:message
void groupChat(int clientfd, std::string str);
// "loginout" command handler
void logout(int clientfd, std::string);

// 系统支持的客户端命令列表
std::unordered_map<std::string, std::string> commandMap = {
    {"help", "显示所有支持的命令, 格式help"},
    {"chat", "一对一聊天, 格式chat:friendid:message"},
    {"addfriend", "添加好友, 格式addfriend:friendid"},
    {"creategroup", "创建群组, 格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式addgroup:groupid"},
    {"groupchat", "群聊, 格式groupchat:groupid:message"},
    {"logout", "注销, 格式logout"}
};

// 注册系统支持的客户端命令处理
std::unordered_map<std::string, std::function<void(int, std::string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addFriend},
    {"creategroup", createGroup},
    {"addgroup", addGroup},
    {"groupchat", groupChat},
    {"logout", logout}
};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buf[1024] = {0};
    while(isMainMenuRunning)
    {
        std::cin.getline(buf, sizeof buf);
        std::string commandBuf(buf);
        std::string command;

        int idx = commandBuf.find(':');
        if (idx == -1)
        {
            command = commandBuf;
        }
        else
        {
            command = commandBuf.substr(0, idx);
        }

        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            std::cerr << "invalid input command!" << std::endl;
            continue;
        }
        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandBuf.substr(idx + 1, commandBuf.size() - idx - 1));
    }
}

// "help" command handler
void help(int, std::string)
{
    std::cout << "show command list >>>" << std::endl;
    for (auto &p : commandMap)
    {
        std::cout << p.first << " : " << p.second << std::endl;
    }
    std::cout << std::endl;
}

// "addfriend" command handler
void addFriend(int clientfd, std::string str)
{
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = atoi(str.c_str());
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (len == -1)
    {
        std::cerr << "send addfriend message error -> " << buffer << std::endl;
    }
}

// "chat" command handler   str格式为：friendid:message
void chat(int clientfd, std::string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        std::cerr << "chat command invalid!" << std::endl;
        return;
    }

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = atoi(str.substr(0, idx).c_str());
    js["msg"] = str.substr(idx + 1, str.size() - idx - 1);
    js["time"] = getCurrentTime();
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (len == -1)
    {
        std::cerr << "send chat message error -> " << buffer << std::endl;
    }
}

// "creategroup" command handler  groupname:groupdesc
void createGroup(int clientfd, std::string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        std::cerr << "creategroup command invalid!" << std::endl;
        return;
    }

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = str.substr(0, idx);
    js["groupdesc"] = str.substr(idx + 1, str.size() - idx - 1);
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (len == -1)
    {
        std::cerr << "send creategroup message error -> " << buffer << std::endl;
    }
}

// "addgroup" command handler
void addGroup(int clientfd, std::string str)
{
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = atoi(str.c_str());
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (len == -1)
    {
        std::cerr << "send addgroup message error -> " << buffer << std::endl;
    }
}

// "groupchat" command handler   groupid:message
void groupChat(int clientfd, std::string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        std::cerr << "groupchat command invalid!" << std::endl;
        return;
    }

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = atoi(str.substr(0, idx).c_str());
    js["msg"] = str.substr(idx + 1, str.size() - idx - 1);
    js["time"] = getCurrentTime();
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (len == -1)
    {
        std::cerr << "send groupchat message error -> " << buffer << std::endl;
    }
}

// "loginout" command handler
void logout(int clientfd, std::string)
{
    json js;
    js["msgid"] = LOGOUT_MSG;
    js["id"] = g_currentUser.getId();
    std::string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (len == -1)
    {
        std::cerr << "send logout message error -> " << buffer << std::endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

// 获取系统时间（聊天信息需要添加时间信息）
std::string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}