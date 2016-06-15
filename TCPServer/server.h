/*************************************************
Copyright: RemoteControl
Author: zcdoyle
Date: 2016-06-13
Description：TCP 收发模块
**************************************************/

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include "TCPCodec.h"
#include "ProtobufCodec.h"
#include "SmartCity.ProtoMessage.pb.h" //TODO：Update
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
using namespace SmartCity; //TODO:Update
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
    typedef uint32_t DEVID;

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
    inline TcpConnectionPtr getDevConnection(int devid)
    {
        MutexLockGuard lock(devConnMutex_);
        map<DEVID, TcpConnectionPtr>::const_iterator it = devToConn_.find(devid);
        if(it != devToConn_.end())
            return it->second;
        else
            return TcpConnectionPtr(NULL);
    }


    /***************************************************
    Description:    发送信息帧，并设置以帧计数为标记的定时器
    Input:          conn：TCP连接
    Output:         无
    Return:         无
    ***************************************************/
    inline void sendWithTimer(TcpConnectionPtr conn, MessageType type, uint16_t totalLength, shared_ptr<u_char> message) //TODO:need lightid?
    {
        weak_ptr<TcpConnection> weakTcpPtr(conn);
        function<void ()> retryExceedHandler = bind(&TCPServer::retryExceedMaxNumer, this, weakTcpPtr);
        dispatcher_.setTimer(conn, totalLength, type, get_pointer(message), retryExceedHandler);
    }

    inline void retryExceedMaxNumer(weak_ptr<TcpConnection> weakConn)
    {
        TcpConnectionPtr conn(weakConn.lock());
        if (conn)
        {
            conn->shutdown();
        }
    }

    //void clearConnectionInfo(const TcpConnectionPtr conn, MessageType messageType); //TODO:need this function?

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
    inline void updateConnectionInfo(TcpConnectionPtr& conn,int devid)
    {
        MutexLockGuard lock(devConnMutex_);
        devToConn_[devid] = conn;
        connHasDev_[conn] = devid;
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
    map<DEVID, TcpConnectionPtr> devToConn_;                    //设备唯一号 -> TCP连接的映射
    map<TcpConnectionPtr, DEVID > connHasDev_;                  //TCP连接 -> 设备唯一号的映射
};

#endif


