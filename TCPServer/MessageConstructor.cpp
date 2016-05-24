/*************************************************
Copyright: SmartLight
Author: albert
Date: 2016-01-13
Description：生成各类帧
**************************************************/

#include "MessageConstructor.h"
#include "EEPROMStruct.h"

/*************************************************
Description:    填充消息字
Calls:          MessageConstructor:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
Output:         信息字帧
Return:         无
*************************************************/
void MessageConstructor::setFrameMessage(FrameMessage& frameMessage, uint8_t tableNumber, uint16_t code, uint8_t type, bool clearData)
{
    frameMessage.tableNumber = tableNumber;
    frameMessage.code = code;
    frameMessage.type = type;
    if(clearData)
        memset(frameMessage.content.data , 0 , sizeof(frameMessage.content.data));
}

/*************************************************
Description:    确认回复帧
Input:          message：需要填入帧内容的空间
                frameCount：确认帧计数
Output:         确认信息字帧
Return:         无
*************************************************/
void MessageConstructor::confirmFrame(u_char* message, int frameCount)
{
    FrameMessage frameMessage;
    frameMessage.tableNumber = 0x00;
    frameMessage.code = 0x0001;
    frameMessage.type = 0x02;
    frameMessage.content.confirm.cnt = frameCount;
    frameMessage.content.confirm.reserve = 0;

    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    校时信息帧
Input:          message：需要填入帧内容的空间
                year：年
                monAndDay：月日
                time：时间
Output:         校时帧
Return:         无
*************************************************/
void MessageConstructor::timingFrame(u_char* message, int year, int monAndDay, int time)
{
    FrameMessage frameMessage;
    frameMessage.tableNumber = 0x00;
    frameMessage.code = 0x0001;
    frameMessage.type = 0x02;
    frameMessage.content.time.year = year;
    frameMessage.content.time.monAndDay = monAndDay;
    frameMessage.content.time.t = time;

    memcpy(message, &frameMessage, sizeof(frameMessage));
}


/*************************************************
Description:    搜索所在组网的所有设备帧
Input:          message：需要填入帧内容的空间
Output:         搜寻组网帧
Return:         无
*************************************************/
void MessageConstructor::searchEquipment(u_char* message)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x04, 0xFFFF, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    查询单独灯杆设备帧
Input:          message：需要填入帧内容的空间
Output:         查询单灯设备帧
Return:         无
*************************************************/
void MessageConstructor::inquireLight(u_char *message)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x04, 0xFFFE, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    查询配置项帧
Input:          message：需要填入帧内容的空间
Output:         查询配置帧
Return:         无
*************************************************/
void MessageConstructor::inquireConfiguration(u_char* message, uint16_t code)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x04, code, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    查询路灯电力信息
Input:          message：需要填入帧内容的空间
Output:         查询电力帧
Return:         无
*************************************************/
void MessageConstructor::inquireLightPower(u_char* message)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x11, 0x0001, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    查询路灯PWM信息帧
Input:          message：需要填入帧内容的空间
Output:         查询单灯PWM帧
Return:         无
*************************************************/
void MessageConstructor::inquireLightPWM(u_char* message)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x11, 0x0002, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    查询环境信息帧
Input:          message：需要填入帧内容的空间
Output:         查询环境帧
Return:         无
*************************************************/
void MessageConstructor::inquireEnvirionment(u_char* message)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x12, 0x0001, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    查询人群信息帧
Input:          message：需要填入帧内容的空间
Output:         查询人群帧
Return:         无
*************************************************/
void MessageConstructor::inquireHuman(u_char* message)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x14, 0x0001, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    查询辅助电源帧
Input:          message：需要填入帧内容的空间
Output:         查询辅助电源帧
Return:         无
*************************************************/
void MessageConstructor::inquirePower(u_char* message)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x1E, 0x0001, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    获取EEPROM信息
Input:          message：需要填入帧内容的空间
                code：EEPROM编码
Output:         获取EEPROM帧
Return:         无
*************************************************/
void MessageConstructor::getEEPROM(u_char* message, int code)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x04, code, 0xF0, true);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}


/*************************************************
Description:    更新EEPROM帧
Input:          message：需要填入帧内容的空间
                eepromData：eeprom内容
                eepromCode：eeprom编码
Output:         更新EEPROM帧
Return:         无
*************************************************/
void MessageConstructor::updateEEPROM(u_char* message, u_char* eepromData, uint16_t eepromCode)
{
    FrameMessage frameMessage;
    setFrameMessage(frameMessage, 0x04, eepromCode, 0xF8,false);
    memcpy(frameMessage.content.data, eepromData, MessageContentLength);
    memcpy(message, &frameMessage, sizeof(frameMessage));
}

/*************************************************
Description:    灯手动控制信息帧
Input:          message：需要填入帧内容的空间
                tableNum：表号
                port：端口
                power：功耗
                duration：持续时间
Output:         灯控制帧
Return:         无
*************************************************/
void MessageConstructor::manualControl(u_char* message, uint8_t tableNum, int port, int power, int duration)
{
    FrameMessage msg;
    setFrameMessage(msg, tableNum, 0x0001, 0xF8, false);

    EEPROMPWMTiming cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.port = port;
    cfg.power = power;
    cfg.lastMin = duration;

    memcpy(msg.content.data, &cfg, sizeof(cfg));
    memcpy(message, &msg, sizeof(msg));
}

/*************************************************
Description:    灯计划控制信息帧
Input:          message：需要填入帧内容的空间
                code：编码
                port：端口
                power：功耗
                startMin：开始时刻
                duration：持续时间
Output:         更新EEPROM帧（计划定时设置）
Return:         无
*************************************************/
void MessageConstructor::planControl(u_char* message, uint8_t code, int port, int power,
                                     uint16_t startMin, uint16_t duration)
{
    FrameMessage msg;
    setFrameMessage(msg, 0x04, code, 0xF8, false);

    EEPROMPWMTiming cfg;
    cfg.port = port;
    cfg.power = power;
    cfg.startTime = startMin;
    cfg.lastMin = duration;

    memcpy(msg.content.data, &cfg, sizeof(cfg));
    memcpy(message, &msg, sizeof(msg));
}

/*************************************************
Description:    语音控制信息帧
Input:          message：需要填入帧内容的空间
                devNum：设备编号
                total：总数
                played：播放数
                times：播放次数
Output:         语音控制帧
Return:         无
*************************************************/
void MessageConstructor::soundControl(u_char* message, int devNum, int total, int played,int times)
{
    FrameMessage msg;
    setFrameMessage(msg, 0x15, 0x0001, 0xF2, false);

    msg.content.soundCtl.senNo = devNum;
    msg.content.soundCtl.total = total;
    msg.content.soundCtl.played = played;
    msg.content.soundCtl.times = times;

    memcpy(message, &msg, sizeof(msg));
}



