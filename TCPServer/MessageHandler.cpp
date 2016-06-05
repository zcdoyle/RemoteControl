/*************************************************
Copyright: SmartLight
Author: albert
Date: 2016-01-16
Description: 消息处理器，完成具体消息的操作
**************************************************/

#include "MessageHandler.h"
#include "MessageConstructor.h"
#include "RpcClient.h"
#include "server.h"
#include "EEPROMStruct.h"
#include "time.h"
#include <sstream>

//Redis回复的资源管理类，用于安全释放资源
class RedisReply
{
public:
    explicit RedisReply(redisReply* reply): reply_(reply) {}

    ~RedisReply()
    {
        freeReplyObject(reply_);
    }

private:
    redisReply* reply_;
};

/*************************************************
Description:    把帧头时间转换成MySQL时间标准格式
Calls:          MessageHandler::
Input:          frameHeader: 帧头指针
Output:         data: 日期(如 2016-1-5)
                time: 时间（如 12:30:12）
Return:         无
*************************************************/
void MessageHandler::getMySQLDateTime(shared_ptr<FrameHeader>& frameHeader, char* date, char* time)
{
    time_t timep;
    struct tm *p;
    time(&timep);
    p=gmtime(&timep);

    sprintf(date, "%04d-%02d-%02d", 1900+p->tm_year, 1+p->tm_mon, p->tm_mday);

    int hour = p->tm_hour;
    int min = p->tm_min;
    int sec = p->tm_sec;
    sprintf(time, "%02d:%02d:%02d", hour, min, sec);
}

/*************************************************
Description:    把帧头时间转换成HBase时间标准格式
Calls:          MessageHandler::
Input:          frameHeader: 帧头指针
Output:         data: 日期(如 20160105)
                time: 时间（如 1230）
Return:         无
*************************************************/
void MessageHandler::getHBaseDateTime(shared_ptr<FrameHeader>& frameHeader, char* date, char* time)
{
    time_t timep;
    struct tm *p;
    time(&timep);
    p=gmtime(&timep);

    sprintf(date, "%04d%02d%02d", 1900+p->tm_year, 1+p->tm_mon, p->tm_mday);

    int hour = p->tm_hour;
    int min = p->tm_min;
    sprintf(time, "%02d%02d", hour, min);
}



/*************************************************
Description:    接收到开关状态信息帧
Calls:          Dispatcher::lightMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onOpenModeMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{

}

/*************************************************
Description:    接收到传感器信息帧
Calls:          Dispatcher::lightMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onSensorMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    uint16_t pm2p5, temperature, humidity;
    uint_32 senId;
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));

    senId = frameHeader->hard;

    ProtoMessage protoMessage;
    initializeHBaseProto(frameHeader, message, protoMessage, SENSOR_MSG, senId);

    pm2p5 = msg.content.sensordata.pm;
    temperature = msg.content.sensordata.temp;
    humidity = msg.content.sensordata.humi;

    protoMessage.set_devnum(senId);
    ProtoMessage_Environment* sensor = protoMessage.mutable_sensor();
    sensor->set_temperature(temperature / 10.0);
    sensor->set_humidity(humidity / 10.0);
    sensor->set_pm2p5(pm2p5 / 10.0);
    tcpserver_->sendProtoMessage(protoMessage, HBase);

    //在Redis更新设备信息
    char command[256];
    sprintf(command, "HMSET ENV%06d temperature %f humidity %f pm2p5 %f",
            senId, temperature / 10.0, humidity / 10.0, pm2p5 / 10.0);
    RedisReply reply((redisReply*)redisCommand(tcpserver_->redisConn_,command));

    LOG_DEBUG << "Environment: " << senId <<
                " temperatur: " << temperature / 10.0 <<
                " humidity: " << humidity / 10.0 <<
                " pm2.5 " << pm2p5 / 10.0;
}


/*************************************************
Description:    初始化HBase信息帧
Calls:          MessageHandler::
Input:          frameHeader: 帧头指针
                message: 消息字指针
                protoMessage：protobuf消息引用
                dev：设备id
Output:         信息字编号
Return:         无
*************************************************/
uint16_t MessageHandler::initializeHBaseProto(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message,
                                              ProtoMessage& protoMessage, MessageType type, int devId)
{
    uint16_t code; //TODO make one by myself
    char date[16], time[16];
    getHBaseDateTime(frameHeader, date, time);
    protoMessage.set_messagetype(type);
    protoMessage.set_messagecode(code);
    protoMessage.set_devnum(devId);
    protoMessage.set_date(date);
    protoMessage.set_time(time);

    return code;
}
