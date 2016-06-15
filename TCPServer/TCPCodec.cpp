/*************************************************
Copyright: SmartLight
Author: albert
Date: 2015-12-16
Description: 编码解码TCP信息帧
**************************************************/

#include "TCPCodec.h"
#include "malloc.h"
#include "stdio.h"
#include "string.h"
#include <sys/time.h>
#include "aes.h"
unsigned char key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};	// 密钥

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
    while(buf->readableBytes() >= sizeof(int64_t)) //TODO：确定循环条件。循环读取直到buffer的内容不够一条完整的帧头+长度
    {
        const void* data = buf->peek();
        //帧头
        shared_ptr<FrameHeader> frameHeader(new FrameHeader);
        memcpy(get_pointer(frameHeader), data, sizeof(FrameHeader));

        uint32_t header = frameHeader->head;
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
        else if(buf->readableBytes() >= (uint16_t)length)
        {
            //帧信息字
            shared_ptr<u_char > message((u_char *)malloc(length - HeaderLength));
            memcpy(get_pointer(message), (u_char *)data + HeaderLength, length - HeaderLength);

            //检查帧头CRC16校验信息
            uint16_t crc16 = CRC16((u_char *)get_pointer(frameHeader), HeaderLength);
            //检查数据CRC32校验，然后解密
            uint16_t crc32 = CRC32(get_pointer(message),length - HeaderLength);
            bool crc16_ok,crc32_ok;

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
                Decrypt(get_pointer(message), key, length-HeaderLength);
                //打印这一帧，调试用
                printFrame("Receive", get_pointer(frameHeader), get_pointer(message), (size_t)(length - HeaderLength));

                //收到一个完整的帧，交给回调函数处理
                buf->retrieve(length); //将buf中length长度的内容清掉
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
                seq: 帧序号
                message: 帧消息字
                source: 信源
                destination: 信宿
Output:         无
Return:         无
*************************************************/
void TCPCodec::send(TcpConnectionPtr conn, uint16_t totalLength, uint16_t type,uint32_t seq, u_char * message)
{
    int mLength=totalLength - HeaderLength; //信息字加密前的长度
    int encryLength = computeEncryptedSize(mLength);   //信息字加密后的长度
    shared_ptr<u_char > encryMessage((u_char *)malloc(encryLength));
    FrameHeader sendFrame;
    sendFrame.head = Header;
    sendFrame.len = encryLength + HeaderLength;
    sendFrame.ver = 0;
    sendFrame.dev = 0;
    sendFrame.type = type;

    sendFrame.olen = mLength;
    sendFrame.enc = 1;
    sendFrame.seq = seq;

    sendFrame.hard = 0;

    //计算CRC16,CRC32校验
    sendFrame.headerCheck = CRC16((u_char *)&sendFrame, HeaderLength);
    sendFrame.messageCheck = CRC32(message, mLength);

    //对message加密
    expandText(message, get_pointer(encryMessage), mLength, encryLength);
    Encrypt(get_pointer(encryMessage), key, encryLength);

    Buffer buf;
    buf.append((void *)&sendFrame, sizeof(FrameHeader));
    buf.append(get_pointer(encryMessage), encryLength);
    conn->send(&buf);

    //打印帧内容，调试用
    printFrame("Send",&sendFrame, message, mLength);
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
    sprintf(headerLine, "%s Header: Header:%02x,length:%d,Type:%02x,
                        "frameCount:%d, MessageLength:%d, headerCheck:%02x, messageCheck:%02x", tag.c_str(),
            frame->head, frame->len, frame->type, frame->seq, messageLen, frame->headerCheck, frame->messageCheck);

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
