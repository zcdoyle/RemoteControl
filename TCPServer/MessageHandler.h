/*************************************************
Copyright: SmartLight
Author: albert
Date: 2016-01-16
Description: 消息处理器，完成具体消息的操作
**************************************************/

#ifndef TCPSERVER_MESSAGEHANDLER_H
#define TCPSERVER_MESSAGEHANDLER_H
#include "TCPCodec.h"
#include "SmartCity.ProtoMessage.pb.h"
#include "dispatcher.h"

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

using namespace muduo::net;
using namespace SmartCity;
using boost::shared_ptr;
using boost::bind;
using std::map;
using std::vector;
using std::istream;
using std::stringstream;

//连接的类型，分为MySQL和HBase两种连接，交给不同模块完成数据存取
typedef enum connectionType
{
    MySQL = 0,
    HBase,
}ConnectionType;

class TCPServer;
class RpcClient;

//MySQL RPC的封装类，用于传递参数
class MySQLRpcParam
{
public:
    TcpConnectionPtr conn;
};

class MessageHandler
{
public:
    explicit MessageHandler(TCPServer *tcpserver, RpcClient* rpcClient):
        tcpserver_(tcpserver),
        rpcClient_(rpcClient) {}

    void onTimingMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message);
    void onAlertMessage(const TcpConnectionPtr&conn,shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message);
    void onConfigMessage(const TcpConnectionPtr&conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message);
    void onLightMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message);
    void onEnvMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message);
    void onHumanMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message);
    void onPowerMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message);


private:
    /*************************************************
    Description:    把帧头时间转换成MySQL时间标准格式
    Calls:          MessageHandler::
    Input:          message: 帧消息字
    Output:         消息字编码号
    Return:         无
    *************************************************/
    inline uint16_t getMessageCode(shared_ptr<u_char> message)
    {
        uint16_t code;
        memcpy(&code, get_pointer(message) + 1, sizeof(code));
        return code;
    }

    void getMySQLDateTime(shared_ptr<FrameHeader>& frameHeader, char *date, char *time);
    void getHBaseDateTime(shared_ptr<FrameHeader>& frameHeader, char* date, char* time);
    void getTime(shared_ptr<FrameHeader>& frameHeader, char* time);

    void setSensorId(const TcpConnectionPtr&conn, FrameMessage& msg, int netWorkNumber);

    uint16_t initializeHBaseProto(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message,
                                  ProtoMessage& protoMessage, MessageType type, int devId);

    TCPServer* tcpserver_;
    RpcClient* rpcClient_;
};


#endif //TCPSERVER_MESSAGEHANDLER_H
