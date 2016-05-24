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
    sprintf(date, "%04d-%02d-%02d", frameHeader->year, (frameHeader->monAndDay) / 100, (frameHeader->monAndDay) % 100);

    uint32_t totalSecond = frameHeader->time / 1000;
    uint16_t hour = totalSecond / 3600;
    uint16_t min = (totalSecond - 3600 * hour) / 60;
    uint16_t sec = totalSecond - 3600 * hour - 60 * min;
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
    sprintf(date, "%04d%02d%02d", frameHeader->year, (frameHeader->monAndDay) / 100, (frameHeader->monAndDay) % 100);

    uint32_t totalSecond = frameHeader->time / 1000;
    uint16_t hour = totalSecond / 3600;
    uint16_t min = (totalSecond - 3600 * hour) / 60;
    sprintf(time, "%02d%02d", hour, min);
}


/*************************************************
Description:    收到校时帧，让MySQLProxy记录校时的时间
Calls:          Dispatcher::timing()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onTimingMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
//    //记录校时时间
//    EEPROMTime time;
//    time.year = frameHeader->year;
//    time.monAndDay = frameHeader->monAndDay;
//    time.time = frameHeader->time;

//    FrameMessage msg;
//    memcpy(&msg, get_pointer(message), sizeof(msg));

//    //设置Protobuf并发送到MySQLProxy
//    ProtoMessage protoMessage;
//    protoMessage.set_messagetype(frameHeader->type);
//    protoMessage.set_messagecode(msg.code);
//    protoMessage.set_devnum(frameHeader->source);
//    ProtoMessage_Configuration* conf = protoMessage.mutable_conf();
//    conf->set_eeprom(std::string((char *)(&time), 8));
//    tcpserver_->sendProtoMessage(protoMessage, MySQL);
}

/*************************************************
Description:    收到告警帧，根据不同的消息编码类型记录不同的信息
Calls:          Dispatcher::alert()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onAlertMessage(const TcpConnectionPtr&conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    char date[16], time[16];
    getMySQLDateTime(frameHeader, date, time);

    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));
    uint16_t messageCode = msg.code;

    //设置Protobuf基本信息
    ProtoMessage protoMessage;
    protoMessage.set_messagetype(frameHeader->type);
    protoMessage.set_messagecode(messageCode);
    protoMessage.set_date(date);
    protoMessage.set_time(time);

    switch (messageCode)
    {
        case 0x0001:
        {
            //总电力非正常,记录电流值
            protoMessage.set_devnum(msg.content.alertAll.ligNo);
            ProtoMessage_Alert* alert = protoMessage.mutable_alert();
            alert->set_current(msg.content.alertAll.i / 100.0);
            LOG_DEBUG << "Total current abnormal, light: " << msg.content.alertAll.ligNo <<
                        " current: " << msg.content.alertAll.i / 100.0;
            tcpserver_->sendProtoMessage(protoMessage, MySQL);
            break;
        }
        case 0x0002:
        {
            //灯电流电压非正常，记录电流电压值
            protoMessage.set_devnum(msg.content.alertVI.ligNo);
            ProtoMessage_Alert* alert = protoMessage.mutable_alert();
            alert->set_voltage(msg.content.alertVI.v / 10.0);
            alert->set_current(msg.content.alertVI.i / 100.0);
            LOG_DEBUG << "Current and voltage abnormal: light: " <<
                        msg.content.alertVI.ligNo <<
                        " voltage: " << msg.content.alertVI.v / 10.0 <<
                        " current: " << msg.content.alertVI.i / 100.0;
            tcpserver_->sendProtoMessage(protoMessage, MySQL);
            break;
        }
        case 0x0003:
        {
            //zigbee网络节点非正常
            LOG_DEBUG << "Zigbee abnormal: light:" << msg.content.alertZigbee.ligNo <<
                        " total: " << msg.content.alertZigbee.total <<
                        " abnormal: " << msg.content.alertZigbee.abnormal;
            tcpserver_->sendSearchMessage(conn);
            break;
        }
        case 0x0004:
        {
            //环境传感器非正常，记录温度和PM2.5
            protoMessage.set_devnum(msg.content.alertEnv.senNo);
            ProtoMessage_Alert *alert = protoMessage.mutable_alert();
            alert->set_temperature(msg.content.alertEnv.temperatur / 10.0);
            alert->set_pm2p5(msg.content.alertEnv.pm2p5 / 10.0);
            LOG_DEBUG << "Env alert, light: " << msg.content.alertEnv.senNo <<
                        " temperatur: " <<msg.content.alertEnv.temperatur / 10.0 <<
                        " pm2.5 " << msg.content.alertEnv.pm2p5 / 10.0;

            tcpserver_->sendProtoMessage(protoMessage, MySQL);
            break;
        }
        case 0x0005:
            break;
        case 0x0006:
        {
            //人群传感器数据非正常，记录噪声值
            protoMessage.set_devnum(msg.content.alertHum.senNo);
            protoMessage.mutable_alert()->set_noise(msg.content.alertHum.noise / 10.0);

            LOG_DEBUG << "Human alert: light" << msg.content.alertHum.senNo <<
                        " noise" << msg.content.alertHum.noise / 10.0;

            tcpserver_->sendProtoMessage(protoMessage, MySQL);
            break;
        }
        case 0x0007:
            break;
        default:
            LOG_WARN << "Unknow alert message code";
            break;
    }
}

/*************************************************
Description:    收到配置信息帧，处理信息
Calls:          Dispatcher::configuration()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onConfigMessage(const TcpConnectionPtr &conn, shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    uint16_t code = getMessageCode(message);

    if(code == 0xFFFF)
    {
        //后台搜寻组网内灯杆设备, 逐一获取消息字
        for(size_t i = 0; i < (frameHeader->messageLength) / MessageLength; i++)
        {
            FrameMessage msg;
            memcpy(&msg, get_pointer(message) + i * MessageLength, sizeof(msg));
            setSensorId(conn, msg, (frameHeader->messageLength) / MessageLength);
        }
    }
    else if(code == 0xFFFE)
    {
        //后台查询某灯杆设备
        FrameMessage msg;
        memcpy(&msg, get_pointer(message), sizeof(msg));
        setSensorId(conn, msg, -1);
    }
}

/*************************************************
Description:    通过RPC从MySQL查询或更新灯杆组网消息
Calls:          MessageHandler::configuration()
Input:          frameHeader: 帧头指针
                message: 消息字指针
                netWorkNumber：组网内灯杆数量，-1表示查询单一灯杆设备
Output:         无
Return:         无
*************************************************/
void MessageHandler::setSensorId(const TcpConnectionPtr& conn, FrameMessage& msg, int netWorkNumber)
{
    MySQLRequest request;
    ContentConfig& cfg = msg.content.cfg;

    //获取灯杆号
    request.set_lightnunber(cfg.ligNo);

    //根据配置项设置是否存在各类传感器
    uint8_t environ  = cfg.env, car = cfg.car, human = cfg.human, sound = cfg.sound;
    MySQLRequest_DeviceNumber* requestDev = request.mutable_devnumber();
    environ == 0x00 ? requestDev->set_environment(false):requestDev->set_environment(true);
    car == 0x00 ? requestDev->set_car(false):requestDev->set_car(true);
    human == 0x00 ? requestDev->set_human(false):requestDev->set_human(true);
    sound == 0x00 ? requestDev->set_sound(false):requestDev->set_sound(true);

    if(netWorkNumber > 0)
        requestDev->set_network_number(netWorkNumber);

    LOG_DEBUG << "Configuration, light: " << cfg.ligNo <<
                " env: " << cfg.env <<
                " car: " << cfg.car <<
                " hum: " << cfg.human <<
                " sound: " << cfg.sound;

    //设置MySQL RPC参数，在回调函数更新连接信息
    MySQLResponse* response = new MySQLResponse;
    MySQLRpcParam* param = new MySQLRpcParam;
    param->conn = conn;
    rpcClient_->stub_.getDeviceId(NULL, &request, response,
                                  NewCallback(tcpserver_, &TCPServer::updateDeviceInfo, response, param));
}


/*************************************************
Description:    接收到灯信息帧
Calls:          Dispatcher::lightMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onLightMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    ProtoMessage protoMessage;
    //根据灯杆编号查询灯id
    int lightId = tcpserver_->getDevId(frameHeader->source, LIGHT);
    uint16_t code = initializeHBaseProto(frameHeader, message, protoMessage, LIGHT_MSG, lightId);
    if(code == 0x0001)
    {
        FrameMessage msg;
        memcpy(&msg, get_pointer(message), sizeof(msg));

        uint32_t current = msg.content.ligI.i;
        LOG_DEBUG << "Light info, light: " << lightId <<
                    " current: " << current / 100.0;

        //在Redis更新设备信息
        char command[64];
        sprintf(command, "HMSET LIG%06d current %f", lightId, current / 100.0);
        RedisReply reply((redisReply*)redisCommand(tcpserver_->redisConn_,command));

        ProtoMessage_Light* light = protoMessage.mutable_light();
        light->set_current(current / 100.0);
        tcpserver_->sendProtoMessage(protoMessage, HBase);
    }
    else if(code == 0x0002)
    {
        ProtoMessage_Light* light = protoMessage.mutable_light();
        ProtoMessage_Light_PWM* pwm = light->mutable_pwm();
        //获取3路PWM信息
        for(int i = 0; i < 3; i++)
        {
            FrameMessage msg;
            memcpy(&msg, get_pointer(message) + i * MessageLength, sizeof(msg));
            ContentLigPWM& ligPwm = msg.content.ligPWM;

            pwm->add_pwmpower(ligPwm.pow);
            pwm->add_pwmtime(ligPwm.time);
            pwm->add_ledcurrent(ligPwm.ledCurrent / 100.0);
            pwm->add_ledvoltage(ligPwm.ledVoltage / 100.0);

            char command[256];
            sprintf(command, "HMSET LIG%06d ledVoltage %f ledCurrent %f pwm%dPower %d, pwm%dTime %dmin", lightId,
                    ligPwm.ledVoltage / 10.0, ligPwm.ledCurrent / 100.0, i + 1, ligPwm.pow, i + 1, ligPwm.time);
            RedisReply reply((redisReply*)redisCommand(tcpserver_->redisConn_,command));

            LOG_DEBUG << "PWM" << ligPwm.port <<
                        " power: " << ligPwm.pow <<
                        " ledVoltage: " << ligPwm.ledVoltage / 10.0 <<
                        " time: " << ligPwm.time <<
                        (ligPwm.unit == 0 ? "min" : "ms") <<
                        " ledCurrent: " << ligPwm.ledCurrent / 100.0;
        }
        tcpserver_->sendProtoMessage(protoMessage, HBase);
    }
}

/*************************************************
Description:    接收到环境信息帧
Calls:          Dispatcher::lightMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onEnvMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    uint16_t temperature, humidity;
    uint32_t pm2p5, senId;
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));
    senId = tcpserver_->getDevId(msg.content.envTH.senNo, ENVIRONMENT);

    ProtoMessage protoMessage;
    initializeHBaseProto(frameHeader, message, protoMessage, ENVIRONMENT_MSG, senId);

    temperature = msg.content.envTH.t;
    humidity = msg.content.envTH.humidity;
    memcpy(&msg, get_pointer(message) + MessageLength, sizeof(msg));
    pm2p5 = msg.content.envPm.pm2p5;

    protoMessage.set_devnum(senId);
    ProtoMessage_Environment* env = protoMessage.mutable_environment();
    env->set_temperature(temperature / 10.0);
    env->set_humidity(humidity / 10.0);
    env->set_pm2p5(pm2p5 / 10.0);
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
Description:    接收到人群信息帧
Calls:          Dispatcher::lightMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onHumanMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
    double noise;
    FrameMessage msg;
    memcpy(&msg, get_pointer(message), sizeof(msg));
    noise = msg.content.humNoise.noise / 10.0;
    int senId = tcpserver_->getDevId(msg.content.envTH.senNo, HUMAN);

    ProtoMessage protoMessage;
    initializeHBaseProto(frameHeader, message, protoMessage, HUMAN_MSG, senId);
    protoMessage.set_devnum(senId);
    ProtoMessage_Human* human = protoMessage.mutable_human();
    human->set_noise(noise);
    tcpserver_->sendProtoMessage(protoMessage, HBase);

    LOG_DEBUG << "Human, senNo: " << senId <<
                " noise:" << noise;

    char command[256];
    sprintf(command, "HMSET HUM%06d noise %lf ", senId, noise);
    RedisReply reply((redisReply*)redisCommand(tcpserver_->redisConn_,command));
}

/*************************************************
Description:    接收到辅助电源信息帧
Calls:          Dispatcher::lightMessage()
Input:          frameHeader: 帧头指针
                message: 消息字指针
Output:         无
Return:         无
*************************************************/
void MessageHandler::onPowerMessage(shared_ptr<FrameHeader>& frameHeader, shared_ptr<u_char> message)
{
//    ProtoMessage protoMessage;
//    initializeHBaseProto(frameHeader, message, protoMessage, POWER_MSG);

//    ProtoMessage_Power* power = protoMessage.mutable_power();

    for(int i = 0; i < 5; i++)
    {
        FrameMessage msg;
        memcpy(&msg, get_pointer(message) + i * MessageLength, sizeof(msg));

//        int lightNumber = frameHeader->source;
//        int lightId = tcpserver_->getLightId(lightNumber);
//        power->add_power(msg.content.power.pow);
//        power->add_time(msg.content.power.time);

//        LOG_INFO << "id: " << lightId << " number: " << lightNumber;
//        if(lightId < 0)
//        {
//            LOG_INFO << "no light device found";
//            return;
//        }

//        char command[256];
//        sprintf(command, "HMSET POW%06d relay%dPower %u relay%dTime %u",
//                lightId, i, msg.content.power.pow, i,msg.content.power.time);
//        RedisReply reply((redisReply*)redisCommand(tcpserver_->redisConn_,command));

        LOG_DEBUG << "Relay" << msg.content.power.port <<
                    " power: " << msg.content.power.pow <<
                    " time: " << msg.content.power.time <<
                    (msg.content.power.unit == 0 ? "min" : "ms");
    }

//    tcpserver_->sendProtoMessage(protoMessage, HBase);
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
    uint16_t code = getMessageCode(message);
    char date[16], time[16];
    getHBaseDateTime(frameHeader, date, time);
    protoMessage.set_messagetype(type);
    protoMessage.set_messagecode(code);
    protoMessage.set_devnum(devId);
    protoMessage.set_date(date);
    protoMessage.set_time(time);

    return code;
}
