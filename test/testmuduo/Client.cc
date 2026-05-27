#include "json.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

using json = nlohmann::json;

namespace
{
std::atomic_bool g_running{true};

void recvTask(int clientfd)
{
    char buffer[4096] = {0};
    while (g_running)
    {
        std::memset(buffer, 0, sizeof(buffer));
        int len = recv(clientfd, buffer, sizeof(buffer) - 1, 0);
        if (len > 0)
        {
            std::string raw(buffer, len);
            while (!raw.empty() && (raw.back() == '\n' || raw.back() == '\r'))
            {
                raw.pop_back();
            }

            try
            {
                json response = json::parse(raw);
                std::cout << "[server] " << response.dump(4) << std::endl;
            }
            catch (const std::exception &ex)
            {
                std::cout << "[server raw] " << raw << std::endl;
                std::cout << "parse error: " << ex.what() << std::endl;
            }
        }
        else if (len == 0)
        {
            std::cout << "server closed connection" << std::endl;
            g_running = false;
            break;
        }
        else
        {
            std::cerr << "recv error" << std::endl;
            g_running = false;
            break;
        }
    }
}
}

int main(int argc, char *argv[])
{
    const char *ip = "127.0.0.1";
    uint16_t port = 8080;
    if (argc > 1)
    {
        ip = argv[1];
    }
    if (argc > 2)
    {
        port = static_cast<uint16_t>(std::stoi(argv[2]));
    }

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        std::cerr << "socket create error" << std::endl;
        return -1;
    }

    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip);

    if (connect(clientfd, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1)
    {
        std::cerr << "connect server error" << std::endl;
        close(clientfd);
        return -1;
    }

    std::cout << "connected to " << ip << ':' << port << std::endl;
    std::cout << "input plain text to send, or enter /quit to exit" << std::endl;

    std::thread reader(recvTask, clientfd);

    int msgid = 1;
    std::string line;
    while (g_running && std::getline(std::cin, line))
    {
        if (line == "/quit")
        {
            g_running = false;
            shutdown(clientfd, SHUT_RDWR);
            break;
        }

        if (line.empty())
        {
            continue;
        }

        json request;
        request["msgid"] = msgid++;
        request["message"] = line;
        request["name"] = "test-client";

        std::string data = request.dump() + "\n";
        int len = send(clientfd, data.c_str(), data.size(), 0);
        if (len == -1)
        {
            std::cerr << "send message error" << std::endl;
            g_running = false;
            break;
        }
    }

    g_running = false;
    shutdown(clientfd, SHUT_RDWR);
    if (reader.joinable())
    {
        reader.join();
    }
    close(clientfd);
    return 0;
}
