/*************************************************
Copyright: SmartLight
Author: albert
Date: 2015-12-16
Description: TCPServer 收发模块
**************************************************/

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "TCPCodec.h"
#include "ProtobufCodec.h"
#include "SmartCity.ProtoMessage.pb.h"
#include "dispatcher.h"
#include "MessageHandler.h"
#include "configuration.h"
#include "RpcClient.h"
#include "JsonHandler.h"
#include "json/JsonCodec.h"
#include "MessageConstructor.h"

#include <muduo/base/Logging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadLocalSingleton.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/protorpc/RpcChannel.h>
#include <hiredis/hiredis.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <map>
#include <vector>
#include <stdio.h>


using namespace muduo::net;
using namespace SmartCity;
using boost::shared_ptr;
using boost::bind;
using std::map;
using std::vector;
using std::istream;
using std::stringstream;
using muduo::ThreadLocalSingleton;
using muduo::LogFile;
using muduo::Logger;
using muduo::TimeZone;


class TCPServer : boost::noncopyable
{
    typedef uint32_t DEVNO_TYPE;

    typedef shared_ptr<TcpClient> TcpClientPtr;
    typedef vector<ProtoMessage> Messages;
    typedef ThreadLocalSingleton<map<ConnectionType, TcpConnectionPtr> > LocalConnections;
    typedef ThreadLocalSingleton<map<ConnectionType, TcpClientPtr> > LocalClients;
    typedef ThreadLocalSingleton<map<ConnectionType, Messages> > UnSendMessages;

public:
    TCPServer(EventLoop* loop, const Configuration &config);
    void sendProtoMessage(ProtoMessage message, ConnectionType type);
    void start();


    void updateDeviceInfo(MySQLResponse* response, MySQLRpcParam *param);
    void setupUpdateFrame(vector<u_char>& sendMessage, const char* cfg, uint16_t code);

    /*************************************************
    Description:    根据设备id和设备类型得到TCP连接
    Input:          devId：设备id
                    devType：设备类型
    Output:         无
    Return:         该设备对应的TCP连接
    *************************************************/
    inline TcpConnectionPtr getDevConnection(int devId, DeviceType devType)
    {
        MutexLockGuard lock(devConnMutex_);
        map<DEVNO_TYPE, TcpConnectionPtr>::const_iterator it = devToConn_.find(getDevNoType(devId, devType));
        if(it != devToConn_.end())
            return it->second;
        else
            return TcpConnectionPtr(NULL);
    }


    /*************************************************
    Description:    搜索组网设备
    Input:          conn：TCP连接
    Output:         无
    Return:         无
    *************************************************/
    inline void sendSearchMessage(TcpConnectionPtr conn)
    {
        shared_ptr<u_char> message(static_cast<u_char*>(malloc(MessageLength)));
        MessageConstructor::searchEquipment(message.get());
        sendWithTimerForDC(0x00FFFFFF, conn, CONFIGURE, MinimalFrameLength, message, -1);
    }


    /***************************************************
    Description:    发送信息帧，并设置以信宿和帧类型为标记的定时器
    Input:          conn：TCP连接
    Output:         无
    Return:         无
    ****************************************************/
    inline void sendMessageWithDT(TcpConnectionPtr conn, int devNum,
                                    MessageType type, int totalLength, shared_ptr<u_char> message, int lightId)
    {
        weak_ptr<TcpConnection> weakTcpPtr(conn);
        function<void ()> retryExceedHandler = bind(&TCPServer::retryExceedMaxNumer, this, weakTcpPtr, lightId);
        dispatcher_.setTimerForDT(devNum, conn, totalLength, type, get_pointer(message), retryExceedHandler);
    }


    /***************************************************
    Description:    发送信息帧，并设置以信宿和帧计数为标记的定时器
    Input:          conn：TCP连接
    Output:         无
    Return:         无
    ***************************************************/
    inline void sendWithTimerForDC(ADDRESS destination, TcpConnectionPtr conn,
                                           MessageType type, uint16_t totalLength, shared_ptr<u_char> message, int lightId)
    {
        weak_ptr<TcpConnection> weakTcpPtr(conn);
        function<void ()> retryExceedHandler = bind(&TCPServer::retryExceedMaxNumer, this, weakTcpPtr, lightId);


        dispatcher_.setTimerForDC(destination, conn, totalLength, type, get_pointer(message), retryExceedHandler);
    }

    inline void retryExceedMaxNumer(weak_ptr<TcpConnection> weakConn, int id)
    {
        TcpConnectionPtr conn(weakConn.lock());
        if (conn)
        {
            if (id != -1)
            {
                clearConnectionInfo(conn, NOT_RESPONSE);
            }
            conn->shutdown();
        }
    }

    void clearConnectionInfo(const TcpConnectionPtr conn, MessageType messageType);

    /***************************************************
    Description:    获取设备唯一号 = 设备id * 10 + 设备类型
                    用于唯一确定设备的类型和编号，个位代表设备类型，除以10表示设备id
    Input:          devId：设备id
                    type：设备类型
    Output:         无
    Return:         设备唯一号
    ***************************************************/
    inline DEVNO_TYPE getDevNoType(int devId, DeviceType type)
    {
        return devId * 10 + type;
    }

    /***************************************************
    Description:    根据设备唯一号，获取设备类型
    Input:          devNoType:根据设备唯一号
    Output:         无
    Return:         设备类型
    ***************************************************/
    inline DeviceType getDevType(DEVNO_TYPE devNoType)
    {
        DeviceType type;
        type = DeviceType(devNoType % 10);
        return type;
    }

    /***************************************************
    Description:    根据设备唯一号，获取设备id
    Input:          devNoType:根据设备唯一号
    Output:         无
    Return:         设备id
    ***************************************************/
    inline int getDevId(DEVNO_TYPE devNoType)
    {
        return devNoType / 10;
    }

    /***************************************************
    Description:    求设备编号
    Input:          devId：设备id
                    type：设备类型
    Output:         无
    Return:         设备编号
    ***************************************************/
    inline uint32_t getDevNumber(int devId, DeviceType type)
    {
        if(type == LIGHT)
        {
            MutexLockGuard lock(lightIdToNumberMutex_);
            map<int, int>::const_iterator it = lightIdToNumber_.find(devId);
            if (it == lightIdToNumber_.end())
                return -1;
            else
                return it->second;
        }
        else
        {
            uint32_t devNum = devId * 10 + type;
            return devNum | 0x01000000;
        }

    }

    /***************************************************
    Description:    求设备id
    Input:          devNumber：设备编号
                    type：设备类型
    Output:         无
    Return:         设备id
    ***************************************************/
    inline uint32_t getDevId(int devNumber, DeviceType type)
    {
        if(type == LIGHT)
        {
            MutexLockGuard lock(lightNumberToIdMutex_);
            map<int, int>::const_iterator it = lightNumberToId_.find(devNumber);
            if(it == lightNumberToId_.end())
                return -1;
            else
                return it->second;
        }
        else
        {
            uint32_t devNum = devNumber & 0x00111111;
            return devNum / 10;
        }
    }


    Dispatcher dispatcher_;
    redisContext* redisConn_;

private:
    void onServerConnection(const TcpConnectionPtr& conn);
    void onJsonConnection(const TcpConnectionPtr& conn);
    void onMySQLProxyConnection(const TcpConnectionPtr& conn);
    void onHBaseProxyConnection(const TcpConnectionPtr& conn);
    void connectRedis();
    void threadInit(EventLoop* loop);

    /***************************************************
    Description:    更新设备连接信息
    Input:          devId：设备id
                    type：设备类型
                    devVec:设备唯一号数组
                    conn：对应设备的TCP连接
    Output:         无
    Return:         设备id
    ***************************************************/
    inline void updateConnectionInfo(int devId, DeviceType type, vector<DEVNO_TYPE>& devVec, TcpConnectionPtr& conn)
    {
        DEVNO_TYPE noType = getDevNoType(devId, type);
        devVec.push_back(noType);
        devToConn_[noType] = conn;
    }

    const Configuration& config_;                               //配置内容
    EventLoop* loop_;                                           //时间循环
    TcpServer server_;                                          //tcp消息服务器
    TcpServer jsonMessageServer_;                               //json消息服务器
    TCPCodec tcpCodec_;                                         //tcp帧编码解码器
    ProtobufCodec protoCodec_;                                  //protobuf编码解码器
    RpcClient rpcClient_;                                       //RPC客户端
    MessageHandler messageHandler_;                             //tcp消息处理对象
    JsonHandler jsonHandler_;                                   //json消息处理对象
    JsonCodec jsonCodec_;                                       //json编码解码器


    MutexLock devConnMutex_;                                    //连接共享变量互斥锁
    map<DEVNO_TYPE, TcpConnectionPtr> devToConn_;               //设备唯一号 -> TCP连接的映射
    map<TcpConnectionPtr, vector<DEVNO_TYPE> > connHasDev_;     //TCP连接 -> 设备唯一号数组映射

    MutexLock lightIdToNumberMutex_;                            
    map<int, int> lightIdToNumber_;                             //灯杆id -> 灯杆号映射

    MutexLock lightNumberToIdMutex_;
    map<int, int> lightNumberToId_;                             //灯杆号 -> 灯杆id映射
};

#endif


