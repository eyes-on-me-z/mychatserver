#pragma once

#include <hiredis/hiredis.h>
#include <functional>
#include <string>
#include <atomic>
#include <thread>

/*
redis作为集群服务器通信的基于发布-订阅消息队列时，会遇到两个难搞的bug问题，参考我的博客详细描述：
https://blog.csdn.net/QIANGWEIYUAN/article/details/97895611
*/

// 好像没有处理多线程析构的问题

class Redis
{
public:
    Redis();
    ~Redis();

    Redis(const Redis&) = delete;
    Redis& operator=(const Redis&) = delete;

    // 连接redis服务器 
    bool connect();

    // 向redis指定的通道channel发布消息
    bool publish(int channel, std::string message);

    // 向redis指定的通道channel订阅消息
    bool subscribe(int channel);

    // 向redis指定的通道channel取消订阅消息
    bool unsubscribe(int channel);

    // 初始化向业务层上报通道消息的回调对象
    void initNotifyHandler(std::function<void(int, std::string)> func);

private:
    // 在独立线程中接收订阅通道中的消息
    void observerChannelMessage();

    // hiredis同步上下文对象，负责publish消息
    redisContext *_publishContext;

    // hiredis同步上下文对象，负责subscribe消息
    redisContext *_subscribeContext;

    // 后台线程必须成为 Redis 对象的成员，析构时先通知退出，
    // 再唤醒阻塞的 redisGetReply()，最后 join() 等线程结束。
    std::thread _observerThread;
    std::atomic_bool _stopObserver;

    // 回调操作，收到订阅的消息，给service层上报
    std::function<void(int, std::string)> _notifyMessageHandler;
};