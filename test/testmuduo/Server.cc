#include <mymuduo/TcpServer.h>
#include "json.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <ctime>

using json = nlohmann::json;
using namespace std::placeholders;

namespace
{
std::string currentTimeString()
{
    char buf[32] = {0};
    std::time_t now = std::time(nullptr);
    std::tm *tmNow = std::localtime(&now);
    if (tmNow != nullptr)
    {
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tmNow);
    }
    return buf;
}
}

class JsonServer
{
public:
    JsonServer(EventLoop *loop, const InetAddress &addr, const std::string &name)
        : _loop(loop), _server(loop, addr, name)
    {
        _server.setConnectionCallback(std::bind(&JsonServer::onConnection, this, _1));
        _server.setMessageCallback(std::bind(&JsonServer::onMessage, this, _1, _2, _3));
        _server.setThreadNum(4);
    }

    void start()
    {
        _server.start();
    }

private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            std::cout << "client connected: " << conn->peerAddress().toIpPort() << std::endl;
            return;
        }

        std::cout << "client disconnected: " << conn->peerAddress().toIpPort() << std::endl;
        conn->shutdown();
    }

    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp)
    {
        std::string raw = buf->retrieveAllAsString();
        while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r'))
        {
            raw.pop_back();
        }

        if (raw.empty())
        {
            return;
        }

        std::cout << "server recv: " << raw << std::endl;

        json response;
        try
        {
            json request = json::parse(raw);
            response["code"] = 0;
            response["message"] = "ok";
            response["server_time"] = currentTimeString();
            response["echo"] = request;

            if (request.contains("msgid"))
            {
                response["msgid"] = request["msgid"];
            }
            if (request.contains("name"))
            {
                response["reply"] = std::string("hello, ") + request["name"].get<std::string>();
            }
            else if (request.contains("message"))
            {
                response["reply"] = std::string("server received: ") + request["message"].get<std::string>();
            }
            else
            {
                response["reply"] = "json received";
            }
        }
        catch (const std::exception &ex)
        {
            response["code"] = 1;
            response["message"] = "invalid json";
            response["error"] = ex.what();
            response["server_time"] = currentTimeString();
        }

        conn->send(response.dump() + "\n");
    }

    EventLoop *_loop;
    TcpServer _server;
};

int main(int argc, char *argv[])
{
    uint16_t port = 8080;
    if (argc > 1)
    {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    InetAddress addr(port);
    EventLoop loop;
    JsonServer server(&loop, addr, "JsonServer");

    std::cout << "json server listening on port " << port << std::endl;
    server.start();
    loop.loop();
    return 0;
}
