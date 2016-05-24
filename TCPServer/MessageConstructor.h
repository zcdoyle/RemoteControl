/*************************************************
Copyright: SmartLight
Author: albert
Date: 2016-01-13
Description：生成各类帧
**************************************************/
#ifndef TCPSERVER_MESSAGECONSTRUCTOR_H
#define TCPSERVER_MESSAGECONSTRUCTOR_H

#include <sys/types.h>
#include <string.h>
#include <stdint.h>
#include "TCPCodec.h"

//构造发送帧
class MessageConstructor
{
public:
    static void confirmFrame(u_char* message, int frameCount);
    static void timingFrame(u_char* message, int year, int monAndDay, int time);
    static void searchEquipment(u_char* message);
    static void inquireLight(u_char* message);
    static void inquireConfiguration(u_char* message, uint16_t code);
    static void inquireLightPower(u_char* message);
    static void inquireLightPWM(u_char* message);
    static void inquireEnvirionment(u_char* message);
    static void inquireHuman(u_char* message);
    static void inquirePower(u_char* message);
    static void getEEPROM(u_char* message, int code);
    static void updateEEPROM(u_char* message, u_char* eepromData, uint16_t eepromCode);
    static void manualControl(u_char* message, uint8_t tableNum, int port, int power, int duration);
    static void planControl(u_char* message, uint8_t code, int port, int power, uint16_t startMin, uint16_t duration);
    static void soundControl(u_char* message, int devNum, int total, int played,int times);
private:
    inline static void setFrameMessage(FrameMessage& frameMessage, uint8_t tableNumber, uint16_t code, uint8_t type, bool clearData);
};


#endif //TCPSERVER_MESSAGECONSTRUCTOR_H
