/*************************************************
Copyright: SmartLight
Author: albert
Date: 2016-02-01
Description：帧结构
**************************************************/

#ifndef FRAME_STRUCT_H
#define FRAME_STRUCT_H

#include <stdint.h>

static const uint16_t HeaderLength = 20;                                    //帧头长度
static const uint8_t Header = 0xAA;                                         //帧头
static const uint8_t Dev = 0;


//消息类型
typedef enum MessageType
{
    CONFIRM = 0x00,
    PURIFIER_CTRL = 0x01,
    PURIFIER_MODE = 0x02,
    OPEN_MODE = 0x03,
    SENSOR_MSG = 0x04,
}MessageType;

#pragma pack(1)

//确认帧
typedef struct ContentConfirm{
    uint16_t seq;
}ContentConfirm;


typedef struct ContentCtrl{
    uint8_t isopen;
}ContentCtrl;

typedef struct ContentMode{
    uint8_t mode;
}ContentMode;

typedef struct ContentOpenMode{
    unsigned isopen:1;
    unsigned mode:7;
}ContentOpenMode;

typedef struct ContentSensorData{
    uint16_t pm;
    uint16_t temp;
    uint16_t humi;
}ContentSensorData;

//信息字内容
typedef union MessageContent{
    ContentConfirm confirm;
    ContentCtrl ctrl;
    ContentMode mode;
    ContentOpenMode openmode;
    ContentSensorData sensordata;
}MessageContent;

//帧头格式
typedef struct FrameHeader{
    uint8_t head; //帧头
    unsigned len:11; //帧长
    unsigned ver:5;  //版本
    uint8_t dev;  //设备类型
    uint8_t type; //帧类型
    unsigned olen:11; //加密前长度
    unsigned enc:1;  //是否加密
    unsigned res1:4; //保留字段1
    uint8_t res2; //保留字段2
    uint16_t seq; //帧序号
    uint16_t headerCheck; //帧头CRC16校验
    uint32_t hard; //发送方的硬件编号
    uint32_t messageCheck; //数据段CRC32校验
}FrameHeader;

//信息字
typedef struct FrameMessage{
    MessageContent content;
}FrameMessage;

#pragma pack()

#endif
