/*************************************************
Copyright: RemoteControl
Author: zcdoyle
Date: 2016-06-13
Description：处理JSON消息
**************************************************/

#include "JsonHandler.h"
#include "SmartCity.ProtoMessage.pb.h"
#include "MessageConstructor.h"
#include "MessageHandler.h"
#include "server.h"

using namespace SmartCity;

const char* JsonHandler::OpenControl = "switch";
const char* JsonHandler::ModeControl = "mode";
const char* JsonHandler::TimeControl = "timing";
const char* JsonHandler::ChildLockControl = "child_lock";
const char* JsonHandler::ErrorReminderControl = "error_reminder";
const char* JsonHandler::UpdateControl = "operation_type";


/*************************************************
Description:    根据JSON对象发送不同类型的数据
Calls:          JsonHandler:
Input:          s：字符buffer
                result：结果字段
                msg：内容字段
Output:         无
Return:         无
*************************************************/
void JsonHandler::onJsonMessage(const TcpConnectionPtr& conn, const Document &jsonObject)
{
    if(jsonObject.HasMember(OpenControl))
    {
        openControl(conn, jsonObject);
    }
    else if(jsonObject.HasMember(ModeControl))
    {
        modecontrol(conn, jsonObject);
    }
    else if(jsonObject.HasMember(TimeControl))
    {
        timeControl(conn, jsonObject);
    }
    else if(jsonObject.HasMember(ChildLockControl))
    {
        settingcontrol(conn, jsonObject);
    }
    else if(jsonObject.HasMember(ErrorReminderControl))
    {
        settingcontrol(conn, jsonObject);
    }
    else if(jsonObject.HasMember(UpdateControl))
    {
        updatecontrol(conn, jsonObject);
    }
    else
    {
        return returnJsonResult(conn, false, "unkonw message");
    }
}

/*************************************************
Description:    根据设备id和类型获取TCP连接
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                conn：需要获取的TCP连接
                devNum：需要获取的设备号
                type：设备类型
Output:         无
Return:         是否获取成功
*************************************************/
void JsonHandler::getConnbyDevID(const TcpConnectionPtr& jsonConn, TcpConnectionPtr& conn, int devid)
{
    conn = tcpServer_->getDevConnection(devid);
    if(get_pointer(conn) == NULL)
    {
        LOG_INFO << "no connection found";
        returnJsonResult(jsonConn, false, "device not connected to server");
        return false;
    }
    return true;
}

/*************************************************
Description:    开关控制
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象

Output:         无
Return:         无
*************************************************/
void JsonHandler::openControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    //解析json串
    int opencontrol = jsonObject[OpenControl].GetInt();
    int devid = jsonObject["device_id"].GetInt();

    //get conn by devid
    TcpConnectionPtr conn;
    getConnbyDevID(jsonConn,conn,devid);

    MessageType type;
    type = OPEN_CTRL;

    int MessageLength = 1;
    shared_ptr<u_char> message((u_char*)malloc(MessageLength)); //构造控制开关帧，需要1byte空间
    MessageConstructor::openControl(get_pointer(message),opencontrol);

    function<void()> sendCb = bind(&TCPServer::sendWithTimer, tcpServer_, conn, type, HeaderLength+MessageLength, message); //TODO: conn?
    conn->getLoop()->runInLoop(sendCb);
    returnJsonResult(jsonConn, true);
}

void JsonHandler::modeControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    //解析json串
    int modecontrol = jsonObject[ModeControl].GetInt();
    int devid = jsonObject["device_id"].GetInt();

    //get conn by devid
    TcpConnectionPtr conn;
    getConnbyDevID(jsonConn,conn,devid);

    MessageType type;
    type = MODE_CTRL;

    int MessageLength = 1;
    shared_ptr<u_char> message((u_char*)malloc(1)); //构造控制模式帧，需要1byte空间
    MessageConstructor::modeControl(get_pointer(message),modecontrol);

    function<void()> sendCb = bind(&TCPServer::sendWithTimer, tcpServer_,conn, type, HeaderLength+MessageLength, message);
    conn->getLoop()->runInLoop(sendCb);
    returnJsonResult(jsonConn, true);
}

void JsonHandler::timeControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    //解析json串
    int timecontrol = jsonObject[TimeControl].GetInt();
    int devid = jsonObject["device_id"].GetInt();

    //get conn by devid
    TcpConnectionPtr conn;
    getConnbyDevID(jsonConn,conn,devid);

    MessageType type;
    type = TIME_CTRL;

    int MessageLength = 1;
    shared_ptr<u_char> message((u_char*)malloc(1)); //构造控制模式帧，需要1byte空间
    MessageConstructor::timeControl(get_pointer(message),timecontrol);

    function<void()> sendCb = bind(&TCPServer::sendWithTimer, tcpServer_,conn, type, HeaderLength+MessageLength, message);
    conn->getLoop()->runInLoop(sendCb);
    returnJsonResult(jsonConn, true);
}
void JsonHandler::settingControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    //解析json串
    int childlockcontrol = jsonObject[ChildLockControl].GetInt();
    int erroremindercontrol = jsonObject[ErrorReminderControl].GetInt();
    int devid = jsonObject["device_id"].GetInt();

    //get conn by devid
    TcpConnectionPtr conn;
    getConnbyDevID(jsonConn,conn,devid);

    MessageType type;
    type = MODE_CTRL;

    int MessageLength = 1;
    shared_ptr<u_char> message((u_char*)malloc(1)); //构造控制模式帧，需要1byte空间
    MessageConstructor::settingControl(get_pointer(message),childlockcontrol,erroremindercontrol);

    function<void()> sendCb = bind(&TCPServer::sendWithTimer, tcpServer_,conn, type, HeaderLength+MessageLength, message);
    conn->getLoop()->runInLoop(sendCb);
    returnJsonResult(jsonConn, true);
}
void JsonHandler::updateControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    //解析json串
    int updatecontrol = jsonObject[UpdateControl].GetInt();
    int devid = jsonObject["device_id"].GetInt();

    //get conn by devid
    TcpConnectionPtr conn;
    getConnbyDevID(jsonConn,conn,devid);

    MessageType type;
    type = MODE_CTRL;

    int MessageLength = 1;
    shared_ptr<u_char> message((u_char*)malloc(1)); //构造控制模式帧，需要1byte空间
    MessageConstructor::updateControl(get_pointer(message),updatecontrol);

    function<void()> sendCb = bind(&TCPServer::sendWithTimer, tcpServer_,conn, type, HeaderLength+MessageLength, message);
    conn->getLoop()->runInLoop(sendCb);
    returnJsonResult(jsonConn, true);
}
