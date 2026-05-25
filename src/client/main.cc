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
        std::cin >> choice;
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

            int len = send(clientfd, request.c_str(), request.size() + 1, 0);
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

            int len = send(clientfd, request.c_str(), request.size() + 1, 0);
            if (len == -1)
            {
                std::cerr << "send register message error: " << request << std::endl;
            }

            sem_wait(&rwsem);   // 等待信号量，子线程处理完注册消息会通知
            break;
        }
        case 3:
        {
            
        }
        }
    }

    return 0;
}