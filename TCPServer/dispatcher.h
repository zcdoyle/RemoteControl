/*************************************************
Copyright: SmartLight
Author: albert
Date: 2015-12-17
Description: 消息分发器，根据设备发来不同的帧做对应的处理
**************************************************/

#ifndef TCPSERVER_DISPATCHER_H
#define TCPSERVER_DISPATCHER_H

#include "TCPCodec.h"
#include "FrameStruct.h"

#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Endian.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/TimerId.h>
#include <muduo/net/EventLoop.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <map>

using muduo::net::EventLoop;
using muduo::net::TimerId;
using muduo::MutexLock;
using muduo::MutexLockGuard;
using muduo::net::TcpConnectionPtr;
using muduo::Timestamp;
using std::map;
using boost::function;
using boost::bind;
using boost::weak_ptr;

const static int RETRY_NUMBER = 10;

class Dispatcher : boost::noncopyable
{
public:
    typedef function<void (shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)> MessageCallback;
    //typedef function<void (const TcpConnectionPtr&, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)> ConfiguraCallback;

    /*************************************************
    Description:    Dispatcher构造函数，设置时间循环和编码解码
    Calls:          TCPServer::TCPServer
    Input:          loop: 时间循环指针
                    tc: 编码解码器
                    timeoutSec：超时重发秒数
    Output:         无
    Return:         无
    *************************************************/
    explicit Dispatcher(EventLoop *loop,TCPCodec &tc, int timeoutSec) :
                        loop_(loop),tcpCodec_(tc), timeoutSec_(timeoutSec) {}

    /*************************************************
    Description:    设置各类消息的回调函数，用于消息分发
    Calls:          TCPServer::TCPServer
    Input:          各类回调函数
    Output:         无
    Return:         无
    *************************************************/
    inline void setCallbacks(const ConfiguraCallback& configCb,
                     const MessageCallback& openmodeCb,
                     const MessageCallback& sensorCb)
    {
        configureCallback_ = configCb;
        openmodeCallback_ = openmodeCb;
        sensorCallback_ = sensorCb;
    }

    /*************************************************
    Description:    发送消息
    Calls:          TCPServer::
    Input:          conn: TCP连接
                    totalLength: 帧总长度
                    type: 帧类型
                    message: 帧消息字
                    destination: 信宿
    Output:         无
    Return:         无
    *************************************************/
    inline void send(const TcpConnectionPtr& conn, uint16_t totalLength, MessageType type,shared_ptr<FrameHeader>& frameHeader, u_char * message)
    {
        uint16_t seq = frameHeader->seq + 1; //帧计数加1
        tcpCodec_.send(conn, totalLength, type, seq, message);
    }

    void sendForTimer(weak_ptr<TcpConnection> weakTcpPtr, uint16_t totalLength, MessageType type, u_char * message,
                             uint32_t destination, uint32_t frameCount, shared_ptr<int> retryCount, function<void ()> retryExceedHandler);

    void onStringMessage(const TcpConnectionPtr&, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message, Timestamp);
    void sendConfirmFrame(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader);
    uint32_t getAndSetFrameCount(ADDRESS destination, uint32_t increment);

    void setTimerForDC(ADDRESS destination, TcpConnectionPtr& conn, uint16_t totalLength, MessageType type, u_char * message, function<void ()> retryExceedHandler);
    void setTimerForDT(ADDRESS destination, TcpConnectionPtr& conn, uint16_t totalLength, MessageType type, u_char * message, function<void ()> retryExceedHandler);

    bool cancelTimerForDC(ADDRESS destination, FRAMECOUNT count);
    bool cancelTimerForDT(ADDRESS destination, MessageType type);

private:
    void confirm(ADDRESS source, shared_ptr<u_char>& message);
<<<<<<< HEAD
    void openModeMessage(const TcpConnectionPtr&, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message);
=======
<<<<<<< HEAD
    void openModeMessage(const TcpConnectionPtr&, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message);
=======
    void openMode(const TcpConnectionPtr&, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message);
>>>>>>> 10bdc6d775d6dfa226aae615ec56460d73aa5ef1
>>>>>>> 36d5b7bcd3161db17025c5fe18cf5c7ba76a870a
    void sensorMessage(const TcpConnectionPtr&, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message);

    EventLoop* loop_;
    TCPCodec& tcpCodec_;
    int timeoutSec_;

    /*由于帧格式设计不科学，有些回复帧没有确认帧编号，需要设置2种定时器
        1、以信宿和帧计数区分的定时器
        2、以信宿和帧类型区分的定时器
    */
    //以信宿和帧计数区分的定时器
    MutexLock confirmMutex_;
    map<ADDRESS, map<FRAMECOUNT, TimerId> > confirmTimer_;

    //以信宿和帧类型区分的定时器
    MutexLock statusMutex_;
    map<ADDRESS, map<MessageType, TimerId> > statusTimer_;

    //帧计数器
    MutexLock frameCountMutex_;
    map<ADDRESS , FRAMECOUNT> frameCountMap_;

    MessageCallback openmodeCallback_;
    MessageCallback sensorCallback_;
};


#endif //TCPSERVER_DISPATCHER_H
