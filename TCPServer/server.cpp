/*************************************************
Copyright: SmartLight
Author: albert
Date: 2015-12-16
Description: TCPServer 收发模块
**************************************************/


#include "server.h"
#include "http.h"
#include "JsonHandler.h"
#include <sstream>

/***************************************************
Description:    TCPServer构造函数，初始化各个模块
Input:          loop：时间循环
                config：配置内容
Output:         无
Return:         无
***************************************************/
TCPServer::TCPServer(EventLoop* loop, const Configuration &config)
            : config_(config),
              loop_(loop),
              server_(loop, InetAddress(config.listenPort_), "TCPServer"),
              jsonMessageServer_(loop, InetAddress(config.jsonListenPort_), "JSONMessageServer"),
              dispatcher_(loop, tcpCodec_, config.timeout_sec_),
              tcpCodec_(config.serverAddr_, bind(&Dispatcher::onStringMessage, &dispatcher_, _1, _2, _3, _4)),
              protoCodec_(),
              rpcClient_(loop, InetAddress(config.MySQLProxyAddress_, config.MySQLRPCPort_)),
              messageHandler_(this, &rpcClient_),
              jsonHandler_(&rpcClient_, this, loop),
              jsonCodec_(bind(&JsonHandler::onJsonMessage, jsonHandler_, _1, _2))
{
    server_.setConnectionCallback(bind(&TCPServer::onServerConnection, this, _1));
    server_.setMessageCallback(bind(&TCPCodec::onMessage, &tcpCodec_, _1, _2, _3)); //!!消息编码回调!!
    server_.setThreadNum(config.threadNum_); //设定线程数

    jsonMessageServer_.setConnectionCallback(bind(&TCPServer::onJsonConnection, this, _1));
    jsonMessageServer_.setMessageCallback(bind(&JsonCodec::onMessage, &jsonCodec_, _1, _2, _3));

    //设置各类处理信息的回调函数  !!派发器利用这些回调函数分发不同消息!!
    dispatcher_.setCallbacks(bind(&MessageHandler::onConfigMessage, &messageHandler_, _1, _2, _3),
                             bind(&MessageHandler::onOpenModeMessage, &messageHandler_, _1, _2),
                             bind(&MessageHandler::onSensorMessage, &messageHandler_, _1, _2));

}

/***************************************************
Description:    连接Redis数据库
Calls:          TCPServer::start()
Input:          无
Output:         无
Return:         无
***************************************************/
void TCPServer::connectRedis()
{
    redisConn_  = redisConnect(config_.RedisAddress_,config_.RedisPort_);
    if(redisConn_ != NULL && redisConn_->err)
        LOG_FATAL << "redis connect error" << redisConn_->errstr;

    redisReply *reply;
    reply= (redisReply* )redisCommand(redisConn_, "AUTH %s", config_.RedisPassword_);
    if (reply->type == REDIS_REPLY_ERROR)
        LOG_FATAL << "redis authoricate error" << redisConn_->errstr;

    LOG_INFO << "Redis connect successful";
}

/***************************************************
Description:    启动TCPServer
Input:          无
Output:         无
Return:         无
***************************************************/
void TCPServer::start()
{
    //初始化各线程
    server_.setThreadInitCallback(bind(&TCPServer::threadInit, this, _1));
    server_.start(); //start以后就利用各个注册的回调函数工作

    jsonMessageServer_.start();

    connectRedis();
    rpcClient_.connect();

//    Http::post(config_.smsAddress_, config_.smsPort_, config_.smsPage, "msg=TCPServer模块启动 【智慧路灯】");
}

/***************************************************
Description:    处理新的TCP连接或断开
Input:          无
Output:         无
Return:         无
***************************************************/
void TCPServer::onServerConnection(const TcpConnectionPtr& conn)
{
    conn->setTcpNoDelay(true);
    LOG_INFO << conn->peerAddress().toIpPort() << " -> "
             << conn->localAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if(conn->connected())
    {
        //新建立连接时搜寻该连接所有设备
        sendSearchMessage(conn);
    }
    else
    {
        TcpConnectionPtr c(conn);
        clearConnectionInfo(conn,LOST_CONNECTION);
    }
}

void TCPServer::clearConnectionInfo(const TcpConnectionPtr conn, MessageType messageType)
{
    //清除连接信息
    MutexLockGuard lock(devConnMutex_);
    map<TcpConnectionPtr, vector<DEVNO_TYPE> >::iterator mapIt = connHasDev_.find(conn);
    if(mapIt != connHasDev_.end())
    {
        //获取灯杆id列表
        ProtoMessage protoMessage;
        protoMessage.set_messagetype(messageType);
        ProtoMessage_ConnectionError* errorId = protoMessage.mutable_errorid();
        for(vector<DEVNO_TYPE>::iterator vecIt = mapIt->second.begin(); vecIt != mapIt->second.end(); vecIt++)
        {
            DEVNO_TYPE noType = *vecIt;
            if(getDevType(noType) == LIGHT)
            {
                errorId->add_lightid(getDevId(noType));
            }

            map<DEVNO_TYPE, TcpConnectionPtr>::iterator devToConnIt = devToConn_.find(noType);
            if(devToConnIt != devToConn_.end())
                devToConn_.erase(devToConnIt);
        }
        connHasDev_.erase(mapIt);
        sendProtoMessage(protoMessage, MySQL);
    }
}

/***************************************************
Description:    处理新的MySQLPRoxy连接或者连接断开情况
Input:          conn：TCP连接
Output:         无
Return:         无
***************************************************/
void TCPServer::onMySQLProxyConnection(const TcpConnectionPtr& conn)
{
    if(conn->connected())
    {
        conn->setTcpNoDelay(true);
        LOG_INFO << "Connect to MySQLProxy successfully";
        LocalConnections::instance()[MySQL] = conn;
        //发送以前发送失败的消息
        for(Messages::iterator it = UnSendMessages::instance()[MySQL].begin();
            it != UnSendMessages::instance()[MySQL].end();
            ++it)
        {
            protoCodec_.send(conn, *it);
            //防止过快发送
            usleep(1);
        }
        UnSendMessages::instance()[MySQL].clear();
    }
    else
    {
        LOG_ERROR << "Lost connection to MySQLProxy";
    }
}

/***************************************************
Description:    处理新的json连接或者连接断开情况
Input:          conn：TCP连接
Output:         无
Return:         无
***************************************************/
void TCPServer::onJsonConnection(const TcpConnectionPtr &conn)
{
    LOG_DEBUG <<"JSONConnection: " << conn->peerAddress().toIpPort() << " -> "
             << conn->localAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if(!conn->connected())
        jsonCodec_.cleanup(conn);
}


/***************************************************
Description:    处理新的HBaseProxy连接或者连接断开情况
Input:          conn：TCP连接
Output:         无
Return:         无
***************************************************/
void TCPServer::onHBaseProxyConnection(const TcpConnectionPtr &conn)
{
    if(conn->connected())
    {
        conn->setTcpNoDelay(true);
        LOG_INFO << "Connect to HBaseProxy successfully";
        LocalConnections::instance()[HBase] = conn;
        //发送以前发送失败的消息
        for(Messages::iterator it = UnSendMessages::instance()[HBase].begin();
            it != UnSendMessages::instance()[HBase].end();
            ++it)
        {
            protoCodec_.send(conn, *it);
            //防止过快发送
            usleep(1);
        }
        UnSendMessages::instance()[HBase].clear();
    }
    else
    {
        LOG_ERROR << "Lost connection to HBaseProxy";
    }
}

/***************************************************
Description:    向数据库代理模块（MySQLProxy或者HBaseProxy）发送信息  !!重点!!利用protobuf向数据库发消息!
Calls:          MessageHandler::
Input:          messageToSend：待发送的protobuf message
                type：连接类型
Output:         无
Return:         无
***************************************************/
void TCPServer::sendProtoMessage(ProtoMessage messageToSend, ConnectionType type)
{
    TcpConnectionPtr dbProxyConn = LocalConnections::instance()[type];
    if(dbProxyConn->connected())
    {
        protoCodec_.send(dbProxyConn, messageToSend);
    }
    else
    {
        //保存发送失败的消息
        LOG_INFO << "Save unsend MySQL messages to UnSendMessages";
        UnSendMessages::instance()[type].push_back(messageToSend);
    }
}


/***************************************************
Description:    各线程初始化，每个线程各有一个MySQL客户端和HBase客户端
Calls:          TCPServer::start()
Input:          loop：事件循环
Output:         无
Return:         无
***************************************************/
void TCPServer::threadInit(EventLoop* loop)
{
    //连接MySQL代理服务器
    TcpClientPtr mysqlProxyPtr(new TcpClient(loop, InetAddress(config_.MySQLProxyAddress_, config_.MySQLProxyPort_), "MySQLProxy"));
    LocalClients::instance()[MySQL] = mysqlProxyPtr;
    mysqlProxyPtr->setConnectionCallback(bind(&TCPServer::onMySQLProxyConnection, this, _1));
    mysqlProxyPtr->enableRetry();
    mysqlProxyPtr->connect();
    UnSendMessages::instance()[MySQL].clear();

    //连接Hbase代理服务器
    TcpClientPtr hbaseProxyPtr(new TcpClient(loop, InetAddress(config_.HBaseProxyAddress_, config_.HBaseProxyPort_), "HBaseProxy"));
    LocalClients::instance()[HBase] = hbaseProxyPtr;
    hbaseProxyPtr->setConnectionCallback(
            bind(&TCPServer::onHBaseProxyConnection, this, _1));
    hbaseProxyPtr->enableRetry();
    hbaseProxyPtr->connect();
    UnSendMessages::instance()[HBase].clear();
    LOG_INFO << "Thread init succssful";
}

/***************************************************
Description:    更新连接情况，并向硬件发送配置信息
Calls:          RPCClient::
Input:          response:RPC响应
                param:RPC传参
Output:         无
Return:         无
***************************************************/
void TCPServer::updateDeviceInfo(MySQLResponse* response, MySQLRpcParam *param)
{
    MySQLResponse_DeviceNumber dev = response->devnumber();
    LOG_DEBUG << dev.light_id() << " " << dev.light_number();

    {
        MutexLockGuard lock(lightIdToNumberMutex_);
        lightIdToNumber_[dev.light_id()] = dev.light_number();
    }

    {
        MutexLockGuard lock(lightNumberToIdMutex_);
        lightNumberToId_[dev.light_number()] = dev.light_id();
    }

    vector<DEVNO_TYPE> devVector;
    map<TcpConnectionPtr, vector<DEVNO_TYPE> >::iterator it = connHasDev_.find(param->conn);
    if (it != connHasDev_.end())
    {
        devVector = it->second;
    }

    MutexLockGuard lock(devConnMutex_);
    LOG_DEBUG << "light id: " << dev.light_id();
    updateConnectionInfo(dev.light_id(), LIGHT, devVector, param->conn);

    if(dev.has_environment())
    {
        LOG_DEBUG << "environment: " << dev.environment();
        updateConnectionInfo(dev.environment(), ENVIRONMENT, devVector, param->conn);
    }
    if(dev.has_car())
    {
        LOG_DEBUG << "car " << dev.car();
        updateConnectionInfo(dev.car(), CAR, devVector, param->conn);
    }
    if(dev.has_human())
    {
        LOG_DEBUG << "human " << dev.human();
        updateConnectionInfo(dev.human(), HUMAN, devVector, param->conn);
    }
    if(dev.has_sound())
    {
        LOG_DEBUG << "sound " << dev.sound();
        updateConnectionInfo(dev.sound(),SOUND, devVector, param->conn);
    }

    connHasDev_[param->conn] = devVector;

    LOG_INFO << "Update Connection Infomation successfully";

    vector<u_char> sMessage;
    setupUpdateFrame(sMessage, dev.envconfig().c_str(), 0x0021);
    setupUpdateFrame(sMessage, dev.carconfig().c_str(), 0x0022);
    setupUpdateFrame(sMessage, dev.humanconfig().c_str(), 0x0023);
    setupUpdateFrame(sMessage, dev.soundconfig().c_str(), 0x0024);

    shared_ptr<u_char> sendMessage((u_char*)malloc(sMessage.size()));
    memcpy(get_pointer(sendMessage), &sMessage[0], sMessage.size());
    sendWithTimerForDC(dev.light_number(), param->conn,
                               CONFIGURE_UPDATE, HeaderLength + sMessage.size(), sendMessage, dev.light_id());

    delete param;
}

/***************************************************
Description:    设置更新设备信息帧
Calls:          TCPServer::updateDeviceInfo
Input:          sendMessage:需要发送的帧消息字
                cfg:配置内容
                code:编码
Output:         无
Return:         无
***************************************************/
void TCPServer::setupUpdateFrame(vector<u_char> &sendMessage, const char* cfg, uint16_t code)
{
    uint32_t notFoundFlag;
    memcpy(&notFoundFlag, cfg, sizeof(notFoundFlag));
    if (notFoundFlag != 0x01FFFFFF)
    {
        //没有此设备，则不发送配置内容
        FrameMessage msg;
        msg.tableNumber = 0x04;
        msg.type = 0xF8;
        msg.code = code;
        memcpy(&(msg.content), cfg, MessageContentLength);
        sendMessage.insert(sendMessage.end(),(u_char*)&msg, (u_char*)&msg + MessageLength);
    }
}





