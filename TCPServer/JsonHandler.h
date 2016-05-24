/*************************************************
Copyright: SmartLight
Author: albert
Date: 2016-01-15
Description：处理JSON消息
**************************************************/

#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <muduo/base/Logging.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/TcpClient.h>
#include "RpcClient.h"
#include "EEPROMStruct.h"


using namespace muduo::net;
using muduo::Logger;
using muduo::Timestamp;
using namespace rapidjson;



class TCPServer;
class MySQLRpcParam;

class JsonHandler
{
public:
    explicit JsonHandler(RpcClient* rpc, TCPServer* tcpServer, EventLoop* loop):
        rpcClient_(rpc),
        tcpServer_(tcpServer),
        loop_(loop){}

    void onJsonMessage(const TcpConnectionPtr& conn, const Document &jsonObject);

private:
    /*************************************************
    Description:    生成Json回复字符串
    Calls:          JsonHandler:
    Input:          s：字符buffer
                    result：结果字段
                    msg：内容字段
    Output:         无
    Return:         无
    *************************************************/
    inline const char* setResultJson(StringBuffer& s,bool result, std::string& msg)
    {
        Writer<StringBuffer> writer(s);
        writer.StartObject();
        writer.String("result");
        writer.Bool(result);
        writer.String("msg");
        writer.String(msg.c_str());
        writer.EndObject();
        return s.GetString();
    }

    /*************************************************
    Description:    发送Json字符串作为响应值
    Calls:          JsonHandler:
    Input:          s：字符buffer
                    result：结果字段
                    msg：内容字段
    Output:         无
    Return:         无
    *************************************************/
    inline void returnJsonResult(const TcpConnectionPtr& conn, bool result, std::string msg = "")
    {
        StringBuffer s;
        if(conn->connected())
        {
            conn->send(setResultJson(s, result, msg));
            conn->shutdown();
        }
    }

    
    /*************************************************
    Description:    以十六进制形式显示结果，调试用
    Calls:          JsonHandler:
    Input:          data：消息字段
                    length：消息长度
    Output:         无
    Return:         无
    *************************************************/
    void printEEPROM(const u_char* data, size_t length)
    {
        char resultString[1024];
        for(size_t i = 0; i < length; i++)
            sprintf(resultString + i * 2, "%02x", data[i]);
        resultString[length * 2] = 0;
        LOG_DEBUG << resultString;
    }

    bool getConnAndDevNum(const TcpConnectionPtr& jsonConn, TcpConnectionPtr& conn, int& devNum,int devId, DeviceType type);

    void searchAllEquipment(const TcpConnectionPtr &jsonConn, const Document& jsonObject);
    void searchSingleEquipment(const TcpConnectionPtr &jsonConn, const Document& jsonObject);
    void getLightElectricity(const TcpConnectionPtr &jsonConn, const Document& jsonObject);
    void getLightPWM(const TcpConnectionPtr &jsonConn, const Document& jsonObject);
    void manualControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject);
    void planControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject);
    void getEnvironment(const TcpConnectionPtr& jsonConn, const Document& jsonObject);
    void getHuman(const TcpConnectionPtr& jsonConn, const Document& jsonObject);
    void soundControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject);
    void getPower(const TcpConnectionPtr& jsonConn, const Document& jsonObject);
    void getConfig(const TcpConnectionPtr& jsonConn, const Document& jsonObject);
    void updateConfiguration(const TcpConnectionPtr& conn, const Document& jsonObject);
    void getEEPROM(MySQLResponse* response, MySQLRpcParam *param);

    void test(const TcpConnectionPtr& jsonConn, const Document& document);

    EventLoop* loop_;
    RpcClient* rpcClient_;
    TCPServer* tcpServer_;

    const static char* SearchAllEquipment;
    const static char* SearchSingleEquipment;
    const static char* LightElectricity;
    const static char* LightPWM;
    const static char* ManualControl;
    const static char* PlanControl;
    const static char* GetEnvironment;
    const static char* GetHuman;
    const static char* SoundControl;
    const static char* GetPower;
    const static char* GetConfig;
    const static char* UpdateConfig;
    const static char* Test;
};

#endif
