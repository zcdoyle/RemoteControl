/*************************************************
Copyright: SmartLight
Author: albert
Date: 2015-12-16
Description: 编码解码TCP信息帧
**************************************************/

#include "TCPCodec.h"


/*************************************************
Description:    解码TCP消息帧，检测帧头、帧长度、校验和等
Calls:          TCPServer收到消息时自动调用
Input:          conn: Tcp连接，
                buf: 消息缓冲区指针
                receiveTime： 接收时间戳
Output:         无
Return:         无
*************************************************/
void TCPCodec::onMessage(const TcpConnectionPtr& conn, Buffer *buf, Timestamp receiveTime)
{
    while(buf->readableBytes() >= sizeof(int64_t)) //TODO：确定循环条件。循环读取知道buffer的内容不够一条完整的消息
    {
        const void* data = buf->peek();
        //帧头
        shared_ptr<FrameHeader> frameHeader(new FrameHeader);
        memcpy(get_pointer(frameHeader), data, sizeof(FrameHeader));

        uint8_t header = frameHeader->head;
        uint16_t length = frameHeader->len;

        if(header != Header)
        {
            //非法帧头
            LOG_WARN << "Invade header";
            skipWrongFrame(buf);
            break;
        }
        else if(length < 0 || length > 2047)
        {
            //非法帧长度
            LOG_WARN << "Invade length" << length;
            skipWrongFrame(buf);
            break;
        }
        else if(buf->readableBytes() >= (uint16_t)length) //TODO:确定判断失败条件
        {
            //帧信息字
            shared_ptr<u_char > message((u_char *)malloc(length - HeaderLength));
            memcpy(get_pointer(message), (u_char *)data + HeaderLength, length - HeaderLength);

            //检查帧头CRC16校验信息
            bool crc16_ok = testCRC16((u_char *)get_pointer(frameHeader), HeaderLength - 2);
            //检查数据CRC32校验，然后解密
            bool crc32_ok = testCRC32();

            if(!crc16_ok)
            {
                LOG_WARN << "Header CRC16 error";
                skipWrongFrame(buf);
                break;
            }

            else if(!crc32_ok)
            {
                LOG_WARN << "Message CRC32 error";
                skipWrongFrame(buf);
                break;
            }

            else
            {
                //解密这一帧

                //打印这一帧，调试用
                printFrame("Receive", get_pointer(frameHeader), get_pointer(message), (size_t)(length - HeaderLength));

                //收到一个完整的帧，交给回调函数处理
                buf->retrieve(length);
                dispatcherCallback_(conn, frameHeader, message, receiveTime);
            }
        }
        else
        {
            break;
        }
    }
}

/*************************************************
Description:    编码TCP消息帧头
Calls:          Dispatcher
Input:          conn: Tcp连接，
                totalLength: 消息总长度
                type： 帧类型
                frameCount: 帧序号
                message: 帧消息字
                source: 信源
                destination: 信宿
Output:         无
Return:         无
*************************************************/
void TCPCodec::send(TcpConnectionPtr conn, uint16_t totalLength, uint16_t type,uint32_t frameCount, u_char * message,
          uint32_t destination)
{
    FrameHeader sendFrame;
    sendFrame.header = Header;
    sendFrame.length = totalLength;
    sendFrame.type = type;
    uint16_t year, md;
    uint32_t time;
    getTime(&year, &md, &time);
    sendFrame.year = year;
    sendFrame.monAndDay = md;
    sendFrame.time = time;
    sendFrame.source = serveAddr_;
    sendFrame.destination = destination;
    sendFrame.frameCount = frameCount;
    sendFrame.messageLength = totalLength - HeaderLength;

    //计算校验和
    sendFrame.headerCheck = accumulate((u_char *)&sendFrame, HeaderLength - 2);
    sendFrame.messageCheck = accumulate(message, totalLength - HeaderLength);

    Buffer buf;
    buf.append((void *)&sendFrame, sizeof(FrameHeader));
    buf.append(message, totalLength - HeaderLength);
    conn->send(&buf);

    //打印帧内容，调试用
    printFrame("Send",&sendFrame, message, totalLength - HeaderLength);
}

/*************************************************
Description:    打印帧，调试用
Calls:          TCPCodec
Input:          tag: 打印帧类型
                frame: 帧头指针
                message: 帧消息字指针
Output:         无
Return:         无
*************************************************/
void TCPCodec::printFrame(std::string tag,FrameHeader *frame, u_char* message, size_t messageLen)
{
    char headerLine[256];
    sprintf(headerLine, "%s Header: Header:%02x,length:%d,Type:%02x,year:%d, monAndDay:%d, time:%d, source:%02x, destination:%02x,"
                        "frameCount:%d, MessageLength:%d, headerCheck:%02x, messageCheck:%02x", tag.c_str(),
            frame->header, frame->length, frame->type, frame->year, frame->monAndDay,frame->time,frame->source, frame->destination,
            frame->frameCount, frame->messageLength, frame->headerCheck, frame->messageCheck);

    std::string mess = tag;
    mess += " Message: ";
    char messageLine[4];
    for(size_t i = 0; i < messageLen; i++)
    {
        sprintf(messageLine, "%02x ", message[i]);
        mess.append(messageLine);
    }

    LOG_DEBUG << headerLine;
    LOG_DEBUG << mess;
}


/*************************************************
Description:    获取当前时间，并转化成帧的时间格式
Calls:          TCPCodec::send
Input:          无
Output:         year: 年
                md: 月日, 如1225
                time: 当天从零点开始的毫秒数
Return:         无
*************************************************/
void TCPCodec::getTime(uint16_t* year, uint16_t *md, uint32_t *time)
{
    //时间函数不是线程安全的, 利用互斥锁保护
    MutexLockGuard lock(timeMutex_);
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm *p;
    p = localtime(&tv.tv_sec);

    *year = (1900 + p->tm_year);
    *md = (1 + p->tm_mon) * 100 + p->tm_mday;
    *time = ((p->tm_hour * 60 + p->tm_min) * 60 + p->tm_sec) * 1000 + tv.tv_usec / 1000;
}

/*************************************************
Description:    计算累加和
Calls:          TCPCodec::send, onMessage
Input:          message: 信息内容
                length: 消息长度
Output:         无
Return:         累加和，取低8位
*************************************************/
uint8_t TCPCodec::accumulate(u_char *message, size_t length)
{
    uint64_t sum = 0;
    for(size_t i = 0; i < length; i++)
    {
        sum += message[i];
    }
    return (uint8_t)(sum & 0x00000000000000FF); //低8位
}

/*************************************************
Description:    跳过错误数据帧
Calls:          TCPCodec::onMessage
Input:          buf: 接收缓冲区指针
Output:         无
Return:         无
*************************************************/
void TCPCodec ::skipWrongFrame(Buffer *buf)
{
    //丢弃第一个字节，并循环查找帧头
    buf->retrieveInt8();
    while(buf->readableBytes() >= sizeof(int32_t))
    {
        if(*(int32_t *)(buf->peek()) == Header) //找到帧头，跳出循环
            break;
        else
            buf->retrieveInt8(); //如果还不是帧头，移动到下一个字节继续寻找
    }
}
