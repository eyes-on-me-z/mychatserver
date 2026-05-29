#include "ChatServer.hpp"
#include "ChatService.hpp"

#include <iostream>
#include <signal.h>

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "command invalid! Example: ./ChatServer 127.0.0.1 6000" << std::endl;
        exit(-1);
    }

    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    signal(SIGINT, resetHandler);
    
    EventLoop loop;
    InetAddress addr(port, ip);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    
    return 0;
}