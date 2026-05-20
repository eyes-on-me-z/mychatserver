#include "ChatServer.hpp"

int main()
{
    EventLoop loop;
    InetAddress addr(8080);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    
    return 0;
}