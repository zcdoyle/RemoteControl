/*************************************************
<<<<<<< HEAD
Copyright: RemoteControl_AirPurifier
=======
Copyright: RemoteControl
>>>>>>> f806e3b4d4421dfe22cf15a822e4fa092164840b
Author: zcdoyle
Date: 2016-06-13
Description：消息处理器，完成具体消息的操作
**************************************************/

#include "MessageHandler.h"
#include "MessageConstructor.h"
#include "RpcClient.h"
#include "server.h"
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
                childlock:儿童锁状态
                errorreminder:故障提醒状态
                time: 采集时间
Output:         无
Return:         无
*************************************************/
void MessageHandler::updateStatusDatainRedis(uint32_t DeviceID, uint32_t switchStatus, uint32_t modeStatus, uint32_t windSpeed, uint32_t timing, uint32_t ver, uint32_t childLock, uint32_t errorReminder, char* timeStr)
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
void MessageHandler::updateSensorDatainRedis(uint32_t DeviceID, uint16_t hcho, uint16_t pm2p5, uint16_t temperature, uint16_t humidity, char* timeStr)
{
    char command[256];
    sprintf(command, "HMSET SENSOR%d hcho %f pm2p5 %f temperature %f humidity %f time %s",
            DeviceID, hcho / 1.0, pm2p5 / 1.0, temperature / 1.0, humidity / 1.0, timeStr);
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
void MessageHandler::updateErrorDatainRedis(uint32_t DeviceID, uint32_t fsc, uint32_t ibc, uint32_t ibe, uint32_t uve, char* timeStr)
{
    char command[256];
    sprintf(command, "HMSET ERROR%d fsc %d ibc %d ibe %d uve %d time %s",
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
void MessageHandler::getMySQLDateTime(char* date, char* timestr)
{
    //时间函数不是线程安全的, 利用互斥锁保护
    MutexLockGuard lock(timeMutex_);

    time_t timep;
    struct tm *p;
    time(&timep);
    p=localtime(&timep);

    sprintf(date, "%04d-%02d-%02d", 1900+p->tm_year, 1+p->tm_mon, p->tm_mday);

    int hour = p->tm_hour;
    int min = p->tm_min;
    int sec = p->tm_sec;
    sprintf(timestr, "%02d:%02d:%02d", hour, min, sec);
}

/*************************************************
Description:    把帧头时间转换成HBase时间标准格式
Calls:          MessageHandler::
Input:          frameHeader: 帧头指针
Output:         data: 日期(如 20160105)
                time: 时间（如 1230）
Return:         无
*************************************************/
void MessageHandler::getHBaseDateTime(char* date, char* timestr)
{
    //时间函数不是线程安全的, 利用互斥锁保护
    MutexLockGuard lock(timeMutex_);

    time_t timep;
    struct tm *p;
    time(&timep);
    p=localtime(&timep);

    sprintf(date, "%04d%02d%02d", 1900+p->tm_year, 1+p->tm_mon, p->tm_mday);

    int hour = p->tm_hour;
    int min = p->tm_min;
<<<<<<< HEAD
    int sec = p->tm_sec;
    sprintf(timestr, "%02d%02d%02d", hour, min, sec);
=======
    sprintf(timestr, "%02d%02d", hour, min);
>>>>>>> f806e3b4d4421dfe22cf15a822e4fa092164840b
}

/*************************************************
Description:    把帧头时间转换成Redis时间标准格式
Calls:          MessageHandler::
Input:          frameHeader: 帧头指针
Output:         time: 201606131946
Return:         无
*************************************************/
void MessageHandler::getRedisDateTime(char* timestr)
{
    //时间函数不是线程安全的, 利用互斥锁保护
    MutexLockGuard lock(timeMutex_);

    time_t timep;
    struct tm *p;
    time(&timep);
    p=localtime(&timep);

    sprintf(timestr, "%04d%02d%02d%02d%02d", 1900+p->tm_year, 1+p->tm_mon, p->tm_mday,p->tm_hour,p->tm_min);
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

    isopen = msg.content.status.isopen;
    mode = msg.content.status.mode;
    wspd = msg.content.status.wspd;
    click = msg.content.status.click;
    ermd = msg.content.status.ermd;
    time = msg.content.status.time;
    ver = msg.content.status.ver;

//    ProtoMessage protoMessage;
//    initializeHBaseProto(protoMessage, STATUS_MSG, senId);
//    protoMessage.set_devid(senId);
//    ProtoMessage_Status* status = protoMessage.mutable_status();
//    status->set_open(isopen);
//    status->set_mode(mode);
//    status->set_wspd(wspd);
//    status->set_click(click);
//    status->set_ermd(ermd);
//    status->set_time(time);
//    status->set_ver(ver);
//    tcpserver_->sendProtoMessage(protoMessage, HBase);
<<<<<<< HEAD

    //在Redis更新设备状态信息
    char timeStr[16];
    getRedisDateTime(timeStr);
    updateStatusDatainRedis(senId,isopen,mode,wspd,time,ver,click,ermd,timeStr);

=======

    //在Redis更新设备状态信息
    char timeStr[16];
    getRedisDateTime(timeStr);
    updateStatusDatainRedis(senId,isopen,mode,wspd,time,ver,click,ermd,timeStr);

>>>>>>> f806e3b4d4421dfe22cf15a822e4fa092164840b
    LOG_DEBUG <<"STATUS: " << senId <<
                " isopen: " << isopen <<
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

    hcho = msg.content.sensor.hcho;
    pm2p5 = msg.content.sensor.pm;
    temperature = msg.content.sensor.temp;
    humidity = msg.content.sensor.humi;

//    ProtoMessage protoMessage;
//    initializeHBaseProto(protoMessage, SENSOR_MSG, senId);

//    protoMessage.set_devid(senId);
//    ProtoMessage_Sensor* sensor = protoMessage.mutable_sensor();
//    sensor->set_hcho(hcho / 1.0); //TODO why / 10.0
//    sensor->set_pm2p5(pm2p5 / 1.0);
//    sensor->set_temperature(temperature / 1.0);
//    sensor->set_humidity(humidity / 1.0);
//    tcpserver_->sendProtoMessage(protoMessage, HBase);

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
<<<<<<< HEAD
Description:    接收到错误信息帧
=======
Description:    接收到传感器信息帧
>>>>>>> f806e3b4d4421dfe22cf15a822e4fa092164840b
Calls:          Dispatcher::ErrorMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onErrorMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    uint32_t fsc, ibc, ibe, uve;
    uint32_t senId;
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));

    senId = frameHeader->hard;

    fsc = msg.content.error.fsc;
    ibc = msg.content.error.ibc;
    ibe = msg.content.error.ibe;
    uve = msg.content.error.uve;

//    ProtoMessage protoMessage;
//    initializeHBaseProto(protoMessage, ERROR_MSG, senId);

//    protoMessage.set_devid(senId);
//    ProtoMessage_Error* error = protoMessage.mutable_error();
//    error->set_fsc(fsc);
//    error->set_ibc(ibc);
//    error->set_ibe(ibe);
//    error->set_uve(uve);
//    tcpserver_->sendProtoMessage(protoMessage, HBase);

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
<<<<<<< HEAD
Description:    收到硬件编号信息帧，处理信息
=======
Description:    收到配置信息帧，处理信息
>>>>>>> f806e3b4d4421dfe22cf15a822e4fa092164840b
Calls:          Dispatcher::devidMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onDevidMessage(const TcpConnectionPtr &conn, shared_ptr<FrameHeader>& frameHeader)
{
    uint32_t devid = frameHeader->hard;
    tcpserver_->updateConnectionInfo(conn,devid);

    LOG_DEBUG << "Devid: " << devid;
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
void MessageHandler::initializeHBaseProto(ProtoMessage& protoMessage, MessageType type, int devId)
{
    char date[16], time[16];
    getHBaseDateTime(date, time);
    protoMessage.set_messagetype(type);
    protoMessage.set_devid(devId);
    protoMessage.set_date(date);
    protoMessage.set_time(time);
}
