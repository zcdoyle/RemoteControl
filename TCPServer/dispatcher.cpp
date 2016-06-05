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
        //cancelTimerForDC(destination, frameCount);
        //cancelTimerForDT(destination, type);
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
            confirm(); //receive confirm frame do nothing
            break;
        case OPEN_MODE:
            openMode(conn, frameHeader, message); //receive open_mode frame, return confirm frame and sql
            break;
        case SENSOR_MSG:
            sensorMessage(conn, frameHeader, message);
            break;
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
void Dispatcher::confirm()
{

}

/*************************************************
Description:    处理open/mode消息
Calls:          Dispatcher::
Input:          conn: TCP连接
                frameHeader: 接收帧头
                message: 接收帧消息字
Output:         无
Return:         无
*************************************************/
void Dispatcher::openModeMessage(const TcpConnectionPtr& conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char>& message)
{
    sendConfirmFrame(conn, frameHeader); //return confirm frame
    openmodeCallback_(frameHeader, message); // messageHandler communicate with sql
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
    sendConfirmFrame(conn, frameHeader);
    sensorCallback_(frameHeader, message);
}
