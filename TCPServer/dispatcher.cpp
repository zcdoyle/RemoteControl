/*************************************************
Copyright: SmartLight
Author: albert
Date: 2015-12-17
Description: 消息分发器，根据设备发来不同的帧做对应的处理
**************************************************/

#include "dispatcher.h"
#include "MessageConstructor.h"
#include "MessageConstructor.h"
#include <time.h>
#include <sys/time.h>


/*************************************************
Description:    设置状态回复定时器，以信宿和帧计数区分
Calls:          TCPServer::
Input:          destination: 信宿
                conn：TCP连接
                totalLength：帧总长度
                tpye：帧类型
                message：帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::setTimerForDC(ADDRESS destination, TcpConnectionPtr& conn, uint16_t totalLength,
                               MessageType type, u_char * message, function<void ()> retryExceedHandler)
{
    //帧计数加1，并取消相关定时器
    uint32_t count = getAndSetFrameCount(destination, 1);

    shared_ptr<int> retryCount(new int);
    *retryCount = 0;
    weak_ptr<TcpConnection> weakTcpPtr(conn);
    function<void()> sendFunc = bind(&Dispatcher::sendForTimer,
                                     this, weakTcpPtr, totalLength, type, message, destination, count, retryCount,
                                     retryExceedHandler);
    //创建定时器，并记录id
    TimerId timerId = loop_->runEvery(timeoutSec_, sendFunc);
    cancelTimerForDC(destination, count);
    {
        MutexLockGuard lock(confirmMutex_);
        confirmTimer_[destination][count] = timerId;
    }

    //立即发送
    sendFunc();
}


/*************************************************
Description:    设置状态回复定时器，以信宿和帧类型区分
Calls:          TCPServer::
Input:          destination: 信宿
                conn：TCP连接
                totalLength：帧总长度
                tpye：帧类型
                message：帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::setTimerForDT(ADDRESS destination, TcpConnectionPtr& conn, uint16_t totalLength,
                               MessageType type, u_char * message, function<void ()> retryExceedHandler)
{
    //帧计数加1，并取消相关定时器
    uint32_t count = getAndSetFrameCount(destination, 1);

    shared_ptr<int> retryCount(new int);
    *retryCount = 0;
    weak_ptr<TcpConnection> weakTcpPtr(conn);
    function<void()> sendFunc = bind(&Dispatcher::sendForTimer,
                                     this, weakTcpPtr, totalLength, type, message, destination, count,
                                     retryCount, retryExceedHandler);
    TimerId timerId = loop_->runEvery(timeoutSec_, sendFunc);

    cancelTimerForDT(destination, type);
    {
        MutexLockGuard lock(statusMutex_);
        statusTimer_[destination][type] = timerId;
    }

    sendFunc();
}


/*************************************************
Description:    取消以信宿和帧计数区分的定时器
Calls:          Dispatcher::
Input:          destination: 信宿
                count: 帧计数
Output:         无
Return:         是否设置了定时器
*************************************************/
bool Dispatcher::cancelTimerForDC(ADDRESS destination, FRAMECOUNT count)
{
    //查找对应元素的迭代器
    MutexLockGuard lock(confirmMutex_);
    map<ADDRESS, map<FRAMECOUNT, TimerId> >::iterator it = confirmTimer_.find(destination);
    if(it != confirmTimer_.end())
    {
        map<FRAMECOUNT, TimerId>::iterator ii = it->second.find(count);
        if(ii != it->second.end())
        {
            //找到后取消定时器，删除对应元素
            loop_->cancel(ii->second);
            it->second.erase(count);
            return true;
        }
    }
    return false;
}

/*************************************************
Description:    取消以信宿和帧类型区分的定时器
Calls:          Dispatcher::
Input:          destination: 信宿
                type: 帧类型
Output:         无
Return:         是否设置了定时器
*************************************************/
bool Dispatcher::cancelTimerForDT(ADDRESS destination, MessageType type)
{
    //查找对应元素的迭代器
    MutexLockGuard lock(statusMutex_);
    map<ADDRESS, map<MessageType, TimerId> >::iterator it = statusTimer_.find(destination);
    if(it != statusTimer_.end())
    {
        map<MessageType, TimerId>::iterator ii = it->second.find(type);
        if(ii != it->second.end())
        {
            //找到后取消定时器，删除对应元素
            loop_->cancel(ii->second);
            it->second.erase(type);
            return true;
        }
    }
    return false;
}


/*************************************************
Description:    发送消息
Calls:          TCPServer::
Input:          conn: TCP连接
                totalLength: 帧总长度
                type: 帧类型
                message: 帧消息字
                destination: 信宿
                frameCount：当前帧的帧计数，保存该帧计数用于超时重发
Output:         无
Return:         无
*************************************************/
void Dispatcher::sendForTimer(weak_ptr<TcpConnection> weakTcpPtr, uint16_t totalLength, MessageType type, u_char * message,
                         uint32_t destination, uint32_t frameCount, shared_ptr<int> retryCount,
                              function<void ()> retryExceedHandler)
{
    int retryCnt = *retryCount;
    TcpConnectionPtr conn(weakTcpPtr.lock());
    if(conn && conn->connected() && retryCnt < RETRY_NUMBER)
    {
        *retryCount = retryCnt + 1;
        LOG_DEBUG << "Retry count: " << *retryCount;
        tcpCodec_.send(conn, totalLength, type, frameCount, message, destination);
    }
    else
    {
        //TCP连接已断开，取消相关的定时器
        cancelTimerForDC(destination, frameCount);
        cancelTimerForDT(destination, type);
        if (retryCnt >= RETRY_NUMBER)
        {
            conn->getLoop()->runInLoop(retryExceedHandler);
        }
    }
}

/*************************************************
Description:    收到一个完整的帧,开始根据类型处理
Calls:          TCPCodec::onMessage
Input:          conn: TCP连接
                frameHeader: 帧头
                message: 帧消息字
                timestamp: 接收时间戳
Output:         无
Return:         无
*************************************************/
void Dispatcher::onStringMessage(const TcpConnectionPtr& conn,
                                shared_ptr<FrameHeader>& frameHeader,
                                shared_ptr<unsigned char>& message,
                                Timestamp timestamp)
{
    switch (frameHeader->type)
    {
        case ALERT:
            alert(conn, frameHeader, message);
            break;
        case TIMING:
            timing(conn, frameHeader, message);
            break;
        case CONFIGURE:
            configuration(conn, frameHeader, message);
            break;
        case CONFIRM:
            confirm(frameHeader->source, message);
            break;
        case LIGHT_MSG:
            lightMessage(conn, frameHeader, message);
            break;
        case ENVIRONMENT_MSG:
            environmentMessage(conn, frameHeader, message);
            break;
        case HUMAN_MSG:
            humanMessage(conn, frameHeader, message);
            break;
        case POWER_MSG:
            powerMessage(conn, frameHeader, message);
            break;
        default:
            LOG_WARN << "UnKnow Message Type";
            break;
    }
}

/*************************************************
Description:    根据信源获取并设置帧计数
Calls:          Dispatcher::
Input:          source: 信源
                increment: 帧计数增量
Output:         无
Return:         设置后的帧计数
*************************************************/
uint32_t Dispatcher::getAndSetFrameCount(ADDRESS destination, uint32_t increment)
{
    uint32_t frameCount;
    {
        MutexLockGuard lock(frameCountMutex_);
        if(frameCountMap_.find(destination) == frameCountMap_.end())
        {
            //不存在当前信源
            frameCount = 0;
            frameCountMap_[destination] = 0;
        }
        else
        {

            frameCountMap_[destination] += increment;
            frameCount = frameCountMap_[destination];;
        }
    }

    return frameCount;
}

/*************************************************
Description:    构造并发送确认帧
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧的帧头
Output:         无
Return:         无
*************************************************/
void Dispatcher::sendConfirmFrame(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader)
{    
    u_char message[MessageLength];
    MessageConstructor::confirmFrame(message, frameHeader->frameCount);

    send(conn, MinimalFrameLength, CONFIRM, message, frameHeader->source);
}

/*************************************************
Description:    处理告警信息字
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::alert(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    uint32_t frameCount = getAndSetFrameCount(frameHeader->source, 1);
    sendConfirmFrame(conn, frameHeader);
    alertCallback_(conn, frameHeader, message);
}


/*************************************************
Description:    处理校时消息字
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::timing(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader,  shared_ptr<u_char>& message)
{
    uint16_t year, md;
    uint32_t time;
    tcpCodec_.getTime(&year, &md, &time);
    uint32_t frameCount = getAndSetFrameCount(frameHeader->source, 1);

    u_char sendMessage[MessageLength];
    MessageConstructor::timingFrame(sendMessage, year, md, time);

    tcpCodec_.send(conn, MinimalFrameLength, TIMING, frameCount, sendMessage, frameHeader->source);
}


/*************************************************
Description:    处理配置消息字
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::configuration(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));
    if(msg.code == 0xFFFF)
        sendConfirmFrame(conn, frameHeader);
    cancelTimerForDT(frameHeader->source, CONFIGURE);
    configureCallback_(conn, frameHeader, message);
}


/*************************************************
Description:    处理确认消息
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::confirm(ADDRESS source, shared_ptr<u_char>& message)
{
    //获取所确认的帧计数
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));
    cancelTimerForDC(source, msg.content.confirm.cnt);
}

/*************************************************
Description:    处理灯信息消息
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::lightMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    if(!cancelTimerForDT(frameHeader->source, LIGHT_MSG)) //没有定时器（反馈-确认模式)
        sendConfirmFrame(conn, frameHeader);
    lightMessageCallback_(frameHeader, message);
}

/*************************************************
Description:    处理环境监测消息
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::environmentMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    if(!cancelTimerForDT(frameHeader->source, ENVIRONMENT_MSG)) //没有定时器（反馈-确认模式)
        sendConfirmFrame(conn, frameHeader);
    environmentCallback_(frameHeader, message);
}

/*************************************************
Description:    处理人群监测消息
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::humanMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    if(!cancelTimerForDT(frameHeader->source, HUMAN_MSG)) //没有定时器（反馈-确认模式)
        sendConfirmFrame(conn, frameHeader);
    humanCallback_(frameHeader, message);
}

/*************************************************
Description:    处理语音发布消息
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::powerMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    if(!cancelTimerForDT(frameHeader->source, POWER_MSG)) //没有定时器（反馈-确认模式)
        sendConfirmFrame(conn, frameHeader);
    powerCallback_(frameHeader, message);
}
