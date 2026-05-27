#include "ChatServer.hpp"
#include "ChatService.hpp"

#include <signal.h>

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main()
{
    signal(SIGINT, resetHandler);
    
    EventLoop loop;
    InetAddress addr(8080);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    
    return 0;
}