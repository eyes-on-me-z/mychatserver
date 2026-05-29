#include "Redis.hpp"

#include <iostream>
#include <cstdlib>
#include <sys/socket.h>

Redis::Redis()
    : _publishContext(nullptr), _subscribeContext(nullptr), _stopObserver(false)
{}

Redis::~Redis()
{
    _stopObserver.store(true);

    if (_subscribeContext != nullptr)
    {
        // 用来打断阻塞中的 redisGetReply()
        shutdown(_subscribeContext->fd, SHUT_RDWR);
    }
    if (_observerThread.joinable())
    {
        // join() 确保后台线程退出后，再 redisFree()
        _observerThread.join();
    }
    if (_publishContext != nullptr)
    {
        redisFree(_publishContext);
        _publishContext = nullptr;
    }
    if (_subscribeContext != nullptr)
    {
        redisFree(_subscribeContext);
        _subscribeContext = nullptr;
    }
}

// 连接redis服务器 
bool Redis::connect()
{
    // 负责publish发布消息的上下文连接
    _publishContext = redisConnect("127.0.0.1", 6379);
    if (_publishContext == nullptr)
    {
        std::cerr << "connect redis fail!" << std::endl;
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    _subscribeContext = redisConnect("127.0.0.1", 6379);
    if (_subscribeContext == nullptr)
    {
        std::cerr << "connect redis fail!" << std::endl;
        return false;
    }

    // 在单独的线程中，监听通道上的事件，有消息给业务层进行上报
    _stopObserver.store(false);
    _observerThread = std::thread([this](){
        observerChannelMessage();
    });

    std::cout << "connect redis-server success!" << std::endl;
    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, std::string message)
{
    redisReply *reply = (redisReply*)redisCommand(_publishContext, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr)
    {
        std::cerr << "publish command fail!" << std::endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道channel订阅消息
bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源
    if (REDIS_ERR == redisAppendCommand(this->_subscribeContext, "SUBSCRIBE %d", channel))
    {
        std::cerr << "subscribe command fail" << std::endl;
        return false;
    }

    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while(!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subscribeContext, &done))
        {
            std::cerr << "subscribe command fail!" << std::endl;
            return false;
        }
    }
    // redisGetReply
    return true;
}

// 向redis指定的通道channel取消订阅消息
bool Redis::unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(this->_subscribeContext, "UNSUBSCRIBE %d", channel))
    {
        std::cerr << "unsubscribe command fail!" << std::endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while(!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subscribeContext, &done))
        {
            std::cerr << "unsubscribe command fail!" << std::endl;
            return false;
        }
    }
    return true;
}

// 在独立线程中接收订阅通道中的消息
void Redis::observerChannelMessage()
{
    redisReply *reply = nullptr;
    while(!_stopObserver.load() &&
            REDIS_OK == redisGetReply(_subscribeContext, (void**)&reply))
    {
        // 订阅收到的消息是一个带三元素的数组
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道上发生的消息
            if (_notifyMessageHandler)
            {
                _notifyMessageHandler(atoi(reply->element[1]->str), reply->element[2]->str);
            }
        }

        if (reply != nullptr)
        {
            freeReplyObject(reply);
            reply = nullptr;
        }
    }

    if (reply != nullptr)
    {
        freeReplyObject(reply);
    }
    std::cerr << ">>>>>>>>>>>>>>>>  observer_channel_message quit  <<<<<<<<<<<<<<" << std::endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::initNotifyHandler(std::function<void(int, std::string)> func)
{
    this->_notifyMessageHandler = func;
}