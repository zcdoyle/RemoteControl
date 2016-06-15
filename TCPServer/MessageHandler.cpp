/*************************************************
Copyright: RemoteControl
Author: zcdoyle
Date: 2016-06-13
Description：消息处理器，完成具体消息的操作
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
Description:    更新Redis数据库中，设备的状态信息
Calls:
Input:          DeviceID: 设备硬件编号
                switchStatus: 开关状态
                modeStatus: 模式状态
                windSpeed: 风速档位
                timing: 剩余定时情况
                time: 采集时间
Output:         无
Return:         无
*************************************************/
void MessageHandler::updateStatusDatainRedis(uint32_t DeviceID, uint32_t switchStatus, uint32_t modeStatus, uint32_t windSpeed, uint32_t timing, uint32_t ver, uint32_t childLock, uint32_t errorReminder, string timeStr)
{
    char command[256];
    sprintf(command, "HMSET STATUS%d switch %d mode %d windspeed %d timing %d ver %d childlock %d errorreminder %d time %s",
            DeviceID, switchStatus, modeStatus, windSpeed, timing, ver, childLock, errorReminder, timeStr);
    RedisReply reply((redisReply*)redisCommand(tcpserver_->redisConn_,command));
}

/*************************************************
Description:    更新Redis数据库中，设备的传感器信息
Calls:
Input:          DeviceID: 设备硬件编号
                temperature: 温度信息
                humidity: 湿度信息
                pm2p5: pm2p5信息
                hcho: 甲醛信息
                time: 采集时间
Output:         无
Return:         无
*************************************************/
void MessageHandler::updateSensorDatainRedis(uint32_t DeviceID, uint16_t hcho, uint16_t pm2p5, uint16_t temperature, uint16_t humidity, string timeStr)
{
    char command[256];
    sprintf(command, "HMSET SENSOR%d temperature %f humidity %f pm2p5 %f hcho %f time %s",
            DeviceID, temperature / 1.0, humidity / 1.0, pm2p5 / 1.0, hcho / 1.0, timeStr);
    RedisReply reply((redisReply*)redisCommand(tcpserver_->redisConn_,command));
}

/*************************************************
Description:    更新Redis数据库中，设备的故障和清洗信息
Calls:
Input:          DeviceID: 设备硬件编号
                fsc: 初效过滤网清洗提示
                ibc: 离子箱清洗提示
                ibe: 离子箱故障提示
                uve: UV灯故障提示
                time: 采集时间
Output:         无
Return:         无
*************************************************/
void MessageHandler::updateErrorDatainRedis(uint32_t DeviceID, uint32_t fsc, uint32_t ibc, uint32_t ibe, float uve, string timeStr)
{
    char command[256];
    sprintf(command, "HMSET SETTING%d fsc %d ibc %d ibe %d uve %f time %s",
            DeviceID, fsc, ibc, ibe, uve, timeStr);
    RedisReply reply((redisReply*)redisCommand(tcpserver_->redisConn_,command));
}

/*************************************************
Description:    把帧头时间转换成MySQL时间标准格式
Calls:          MessageHandler::
Input:          frameHeader: 帧头指针
Output:         data: 日期(如 2016-1-5)
                time: 时间（如 12:30:12）
Return:         无
*************************************************/
void MessageHandler::getMySQLDateTime(char* date, char* time)
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
void MessageHandler::getHBaseDateTime(char* date, char* time)
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
Description:    把帧头时间转换成Redis时间标准格式
Calls:          MessageHandler::
Input:          frameHeader: 帧头指针
Output:         time: 201606131946
Return:         无
*************************************************/
void MessageHandler::getRedisDateTime(char* time)
{
    time_t timep;
    struct tm *p;
    time(&timep);
    p=gmtime(&timep);

    sprintf(time, "%04d%02d%02d%02d%02d", 1900+p->tm_year, 1+p->tm_mon, p->tm_mday,p->tm_hour,p->tm_min);
}

/*************************************************
Description:    接收到状态信息帧
Calls:          Dispatcher::StatusMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onStatusMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    uint32_t isopen,mode,wspd,click,ermd,time,ver; //帧中包含的状态
    uint32_t senId;
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));

    senId = frameHeader->hard;

    ProtoMessage protoMessage;
    initializeHBaseProto(frameHeader, message, protoMessage, STATUS_MSG, senId);

    isopen = msg.content.status.isopen;
    mode = msg.content.status.mode;
    wspd = msg.content.status.wspd;
    click = msg.content.status.click;
    ermd = msg.content.status.ermd;
    time = msg.content.status.time;
    ver = msg.content.status.ver;

    protoMessage.set_devnum(senId);
    ProtoMessage_Environment* status = protoMessage.mutable_status();
    status->set_switch(isopen);
    status->set_mode(mode);
    status->set_wspd(wspd);
    status->set_click(click);
    status->set_ermd(ermd);
    status->set_time(time);
    status->set_ver(ver);
    tcpserver_->sendProtoMessage(protoMessage, HBase);
    //在Redis更新设备状态信息
    char timeStr[16];
    getRedisDateTime(timeStr);
    updateStatusDatainRedis(senId,isopen,mode,wspd,time,ver,click,ermd,timeStr);

    LOG_DEBUG <<" isopen: " << isopen <<
                " mode: " << mode <<
                " wspd: " << wspd <<
                " click: " << click <<
                " ermd: " << ermd <<
                " time: " << time <<
                " ver: " << ver<<
                " timeStr: " << timeStr;

}

/*************************************************
Description:    接收到传感器信息帧
Calls:          Dispatcher::SensorMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onSensorMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    uint16_t hcho, pm2p5, temperature, humidity;
    uint32_t senId;
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));

    senId = frameHeader->hard;

    ProtoMessage protoMessage;
    initializeHBaseProto(frameHeader, message, protoMessage, SENSOR_MSG, senId);

    hcho = msg.content.sensor.hcho;
    pm2p5 = msg.content.sensor.pm;
    temperature = msg.content.sensor.temp;
    humidity = msg.content.sensor.humi;

    protoMessage.set_devnum(senId);
    ProtoMessage_Environment* sensor = protoMessage.mutable_sensor();
    sensor->set_hcho(hcho / 1.0); //TODO why / 10.0
    sensor->set_pm2p5(pm2p5 / 1.0);
    sensor->set_temperature(temperature / 1.0);
    sensor->set_humidity(humidity / 1.0);
    tcpserver_->sendProtoMessage(protoMessage, HBase);

    //在Redis更新设备信息
    char timeStr[16];
    getRedisDateTime(timeStr);
    updateSensorDatainRedis(senId, hcho, pm2p5, temperature, humidity, timeStr);

    LOG_DEBUG << "Sensor: " << senId <<
                " hcho: " << hcho / 1.0 <<
                " pm2.5 " << pm2p5 / 1.0 <<
                " temperatur: " << temperature / 1.0 <<
                " humidity: " << humidity / 1.0;
}

/*************************************************
Description:    接收到传感器信息帧
Calls:          Dispatcher::SensorMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onErrorMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    uint16_t fsc, ibc, ibe, uve;
    uint32_t senId;
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));

    senId = frameHeader->hard;

    ProtoMessage protoMessage;
    initializeHBaseProto(frameHeader, message, protoMessage, ERROR_MSG, senId);

    fsc = msg.content.error.fsc;
    ibc = msg.content.error.ibc;
    ibe = msg.content.error.ibe;
    uve = msg.content.error.uve;

    protoMessage.set_devnum(senId);
    ProtoMessage_Environment* error = protoMessage.mutable_error();
    error->set_fsc(fsc);
    error->set_ibc(ibc);
    error->set_ibe(ibe);
    error->set_uve(uve);
    tcpserver_->sendProtoMessage(protoMessage, HBase);

    //在Redis更新设备信息
    char timeStr[16];
    getRedisDateTime(timeStr);
    updateErrorDatainRedis(senId, fsc, ibc, ibe, uve, timeStr);

    LOG_DEBUG << "Error: " << senId <<
                " fsc: " << fsc <<
                " ibc: " << ibc <<
                " ibe: " << ibe <<
                " uve: " << uve;
}

/*************************************************
Description:    收到配置信息帧，处理信息
Calls:          Dispatcher::devid()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onDevidMessage(const TcpConnectionPtr &conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    uint32_t devid = frameHeader->hard;
    tcpserver_->updateConnectionInfo(conn,devid);
}

/*************************************************
Description:    初始化HBase信息帧
Calls:          MessageHandler::
Input:          frameHeader: 帧头指针
                message: 消息字指针
                protoMessage：protobuf消息引用
                dev：设备id
Output:         无
Return:         无
*************************************************/
void MessageHandler::initializeHBaseProto(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message,
                                              ProtoMessage& protoMessage, MessageType type, int devId)
{
    char date[16], time[16];
    getHBaseDateTime(frameHeader, date, time);
    protoMessage.set_messagetype(type);
    protoMessage.set_devnum(devId);
    protoMessage.set_date(date);
    protoMessage.set_time(time);
}
