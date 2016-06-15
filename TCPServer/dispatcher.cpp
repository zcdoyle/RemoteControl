/*************************************************
Copyright: RemoteControl
Author: zcdoyle
Date: 2016-06-13
Description：消息分发器，根据设备发来不同的帧做对应的处理
**************************************************/

#include "dispatcher.h"
#include "MessageConstructor.h"
#include "MessageConstructor.h"
#include <time.h>
#include <sys/time.h>


/*************************************************
Description:    设置状态回复定时器，以帧计数区分
Calls:          TCPServer::
Input:          destination: 信宿
                conn：TCP连接
                totalLength：帧总长度
                tpye：帧类型
                message：帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::setTimer(TcpConnectionPtr& conn, uint16_t totalLength, MessageType type, u_char * message, function<void ()> retryExceedHandler)
{
    //帧计数加1，并取消相关定时器
    uint32_t count = frameCount_++; //TODO:这样做是否合理，原来是看是否存在这个信宿，不存在为0,存在累加；现在就是从0开始累加

    shared_ptr<int> retryCount(new int);
    *retryCount = 0;
    weak_ptr<TcpConnection> weakTcpPtr(conn);
    function<void()> sendFunc = bind(&Dispatcher::sendForTimer,this, weakTcpPtr, totalLength, type, message, count, retryCount,retryExceedHandler);
    //创建定时器，并记录id
    TimerId timerId = loop_->runEvery(timeoutSec_, sendFunc);
    cancelTimer(count);
    {
        MutexLockGuard lock(confirmMutex_);
        confirmTimer_[count] = timerId;
    }
    //立即发送
    sendFunc();
}

/*************************************************
Description:    取消帧计数区分的定时器
Calls:          Dispatcher::
Input:          destination: 信宿
                count: 帧计数
Output:         无
Return:         是否设置了定时器
*************************************************/
bool Dispatcher::cancelTimer(FRAMECOUNT count)
{
    //查找对应元素的迭代器
    MutexLockGuard lock(confirmMutex_);
    map<FRAMECOUNT, TimerId>::iterator it = confirmTimer_.find(count);
    if(it != confirmTimer_.end())
    {
        //找到后取消定时器，删除对应元素
        loop_->cancel(it->second);
        confirmTimer_.erase(count);
        return true;
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
                         uint32_t frameCount, shared_ptr<int> retryCount,function<void ()> retryExceedHandler)
{
    int retryCnt = *retryCount;
    TcpConnectionPtr conn(weakTcpPtr.lock());
    if(conn && conn->connected() && retryCnt < RETRY_NUMBER)
    {
        *retryCount = retryCnt + 1;
        LOG_DEBUG << "Retry count: " << *retryCount;
        tcpCodec_.send(conn, totalLength, type, frameCount, message);
    }
    else
    {
        //TCP连接已断开，取消相关的定时器
        cancelTimer(frameCount);
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
        case CONFIRM:
            confirm(message); //TODO:cancel timeid
            break;
        case STATUS_MSG:
            statusMessage(conn, frameHeader, message); //receive status frame, return confirm frame and sql
            break;
        case SENSOR_MSG:
            sensorMessage(conn, frameHeader, message);
            break;
        case ERROR_MSG:
            errorMessage(conn, frameHeader, message);
            break;
        case DEVID:
            devid(conn, frameHeader, message);
        default:
            LOG_WARN << "UnKnow Message Type";
            break;
    }
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
    u_char message[2]; //confirm frame 16 bit
    MessageConstructor::confirmFrame(message, frameHeader->seq); // messageConstructor make the confirm frame
    uint16_t ConfirmFrameLength = HeaderLength + 2;
    send(conn, ConfirmFrameLength, CONFIRM, frameHeader, message);
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
void Dispatcher::confirm(shared_ptr<u_char>& message)
{
    //获取所确认的帧计数,取消相应定时器
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));
    cancelTimer(msg.content.confirm.seq);
}

/*************************************************
Description:    处理status消息
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::statusMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    //sendConfirmFrame(conn, frameHeader); //return confirm frame
    statusCallback_(frameHeader, message); // messageHandler communicate with sql
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
void Dispatcher::sensorMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    //sendConfirmFrame(conn, frameHeader);
    sensorCallback_(frameHeader, message);
}

void Dispatcher::errorMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    //sendConfirmFrame(conn, frameHeader);
    errorCallback_(frameHeader, message);
}

void Dispatcher::devidMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    sendConfirmFrame(conn, frameHeader);
    devidCallback_(conn,frameHeader, message);
}
