/*************************************************
Copyright: SmartLight
Author: albert
Date: 2016-01-15
Description：处理JSON消息
**************************************************/

#include "JsonHandler.h"
#include "SmartCity.ProtoMessage.pb.h"
#include "MessageConstructor.h"
#include "MessageHandler.h"
#include "server.h"

using namespace SmartCity;

const char* JsonHandler::SearchAllEquipment = "SearchAllEquipment";
const char* JsonHandler::SearchSingleEquipment = "SearchSingleEquipment";
const char* JsonHandler::LightElectricity = "LightElectricity";
const char* JsonHandler::LightPWM = "LightPWM";
const char* JsonHandler::ManualControl = "ManualControl";
const char* JsonHandler::PlanControl = "PlanControl";
const char* JsonHandler::GetEnvironment = "GetEnvironment";
const char* JsonHandler::GetHuman = "GetHuman";
const char* JsonHandler::SoundControl = "SoundControl";
const char* JsonHandler::GetPower = "GetPower";
const char* JsonHandler::GetConfig = "GetConfig";
const char* JsonHandler::UpdateConfig = "UpdateConfig";
const char* JsonHandler::Test = "Test";

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
    if(jsonObject.HasMember(SearchAllEquipment))
    {
        searchAllEquipment(conn, jsonObject);
    }
    else if(jsonObject.HasMember(SearchSingleEquipment))
    {
        searchSingleEquipment(conn, jsonObject);
    }
    else if(jsonObject.HasMember(LightElectricity))
    {
        getLightElectricity(conn, jsonObject);
    }
    else if(jsonObject.HasMember(LightPWM))
    {
        getLightPWM(conn, jsonObject);
    }
    else if(jsonObject.HasMember(ManualControl))
    {
        manualControl(conn, jsonObject);
    }
    else if(jsonObject.HasMember(PlanControl))
    {
        planControl(conn, jsonObject);
    }
    else if(jsonObject.HasMember(GetEnvironment))
    {
        getEnvironment(conn, jsonObject);
    }
    else if(jsonObject.HasMember(GetHuman))
    {
        getHuman(conn, jsonObject);
    }
    else if(jsonObject.HasMember(SoundControl))
    {
        soundControl(conn, jsonObject);
    }
    else if(jsonObject.HasMember(GetPower))
    {
        getPower(conn, jsonObject);
    }
    else if(jsonObject.HasMember(GetConfig))
    {
        getConfig(conn, jsonObject);
    }
    else if(jsonObject.HasMember(UpdateConfig))
    {
        updateConfiguration(conn, jsonObject);
    }
    else if(jsonObject.HasMember(Test))
    {
        test(conn, jsonObject);
    }
    else
    {
        return returnJsonResult(conn, false, "unkonw message");
    }
}

/*************************************************
Description:    根据设备id和类型获取设备编号和TCP连接
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                conn：需要获取的TCP连接
                devNum：需要获取的设备号
                type：设备类型
Output:         无
Return:         是否获取成功
*************************************************/
bool JsonHandler::getConnAndDevNum(const TcpConnectionPtr& jsonConn, TcpConnectionPtr& conn, int& devNum, int devId, DeviceType type)
{
    devNum = tcpServer_->getDevNumber(devId, type);
    if (devNum == -1)
    {
        LOG_WARN << "No such light connnected to TCPServer" << devId;
        return false;
    }

    conn = tcpServer_->getDevConnection(devId, type);
    if(get_pointer(conn) == NULL)
    {
        LOG_INFO << "no connection found";
        returnJsonResult(jsonConn, false, "device not connected to server");
        return false;
    }
    return true;
}


/*************************************************
Description:    搜索所有灯杆设备
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"SearchAllEquipment": [1]}， 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::searchAllEquipment(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    const Value& lights = jsonObject[SearchAllEquipment];
    for(SizeType i = 0; i < lights.Size(); i++)
    {
        int lightId = lights[i].GetInt();
        LOG_DEBUG << SearchAllEquipment << ": " << lightId;

        TcpConnectionPtr conn = tcpServer_->getDevConnection(lightId, LIGHT);
        if(get_pointer(conn) == NULL)
        {
            LOG_INFO << "no connection found";
            returnJsonResult(jsonConn, false, "device not connected to server");
            continue;
        }

        function<void()> sendCb = bind(&TCPServer::sendSearchMessage, tcpServer_, conn);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}


/*************************************************
Description:    搜索所在组网的所有设备
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"SearchSingleEquipment": [1]} 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::searchSingleEquipment(const TcpConnectionPtr &jsonConn, const Document& jsonObject)
{
    const Value& lights = jsonObject[SearchSingleEquipment];
    for(SizeType i = 0; i <lights.Size(); i++)
    {
        int lightId = lights[i].GetInt();
        LOG_DEBUG << SearchSingleEquipment << ": " << lightId;

        int lightNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, lightNumber, lightId, LIGHT))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength));
        MessageConstructor::inquireLight(get_pointer(message));
        function<void()> sendCb = bind(&TCPServer::sendMessageWithDT, tcpServer_,
                                       conn, lightNumber, CONFIGURE, MinimalFrameLength, message, lightId);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    获取路灯电力信息
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"LightElectricity": [1]} 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::getLightElectricity(const TcpConnectionPtr &jsonConn, const Document& jsonObject)
{
    const Value& lights = jsonObject[LightElectricity];
    for(SizeType i = 0; i <lights.Size(); i++)
    {
        int lightId = lights[i].GetInt();
        LOG_DEBUG << LightElectricity << ": " << lightId;

        int lightNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, lightNumber, lightId, LIGHT))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength));
        MessageConstructor::inquireLightPower(get_pointer(message));
        function<void()> sendCb = bind(&TCPServer::sendMessageWithDT, tcpServer_,
                                       conn, lightNumber, LIGHT_MSG, MinimalFrameLength,message, lightId);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    获取路灯PWM信息
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"LightPWM": [1]} 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::getLightPWM(const TcpConnectionPtr &jsonConn, const Document& jsonObject)
{
    const Value& lights = jsonObject[LightPWM];
    for(SizeType i = 0; i <lights.Size(); i++)
    {
        int lightId = lights[i].GetInt();
        LOG_DEBUG << LightPWM << ": " << lightId;

        int lightNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, lightNumber, lightId, LIGHT))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength));
        MessageConstructor::inquireLightPWM(get_pointer(message));
        function<void()> sendCb = bind(&TCPServer::sendMessageWithDT, tcpServer_,
                                       conn, lightNumber, LIGHT_MSG, MinimalFrameLength,message, lightId);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    手工控制路灯
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"DeviceType":1,"ManualControl":[1], "power": 100, "duration": 1} 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::manualControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    const Value& lights = jsonObject[ManualControl];
    int devType = jsonObject["DeviceType"].GetInt();
    int power = jsonObject["power"].GetInt();
    int duration = jsonObject["duration"].GetInt();
    if(devType < 1 || devType > 8)
        return returnJsonResult(jsonConn, false, "device type error");

    //获取表号编码
    uint8_t tableNum;
    MessageType type;
    if(devType <= 3)
    {
        type = LIGHT_CTRL;
        tableNum = 0x11;
    }
    else
    {
        type = POWER_CTRL;
        tableNum = 0x1E;
        power = power > 0 ? 1 : 0;
    }

    shared_ptr<u_char> message((u_char*)malloc(MessageLength));
    MessageConstructor::manualControl(get_pointer(message), tableNum, devType, power, duration);

    for(SizeType i = 0; i <lights.Size(); i++)
    {
        int lightId = lights[i].GetInt();
        LOG_DEBUG << ManualControl << ": " << lightId;

        int lightNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, lightNumber, lightId, LIGHT))
            continue;

        function<void()> sendCb = bind(&TCPServer::sendWithTimerForDC, tcpServer_,
                                       lightNumber, conn, type, MinimalFrameLength, message, lightId);
        conn->getLoop()->runInLoop(sendCb);
    }
    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    计划模型控制路灯
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"PlanControl":
                    [
                        {"light": 1,  "DeviceType": 1, "timing":
                             [{"power": 50, "startMin": 60, "duration": 120},...]//只能是5组，依次为1到5组
                        }
                    ]
                }
Output:         无
Return:         无
*************************************************/
void JsonHandler::planControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    const Value& lights = jsonObject[PlanControl];
    for(SizeType i = 0; i <lights.Size(); i++)
    {
        const Value& light = lights[i];
        int lightId = light["light"].GetInt();
        int devType = light["DeviceType"].GetInt();
        if(devType < 1 || devType > 8)
        {
            returnJsonResult(jsonConn, false, "device type error");
            continue;
        }

        const Value& timingSerial = light["timing"];
        if(timingSerial.Size() != 5)
        {
            LOG_WARN << "plan control json string error, not 5 set";
            returnJsonResult(jsonConn, false, "timing number is not 5");
            continue;
        }

        LOG_DEBUG << PlanControl <<": " << lightId;
        int lightNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, lightNumber, lightId, LIGHT))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength * 5));
        u_char* messagePtr = get_pointer(message);

        for(int j = 0; j < 5; j++)
        {
            const Value& timing = timingSerial[j];
            uint8_t power = timing["power"].GetInt();
            uint16_t startMin = timing["startMin"].GetInt();
            uint16_t duration = timing["duration"].GetInt();
            LOG_INFO << j+1 << " " << power << " " << startMin / 60 << " " << duration;

            MessageConstructor::planControl(messagePtr, 0x002c + devType * 5 + j, devType, power, startMin, duration);

            messagePtr += MessageLength;
        }

        function<void()> sendCb = bind(&TCPServer::sendWithTimerForDC, tcpServer_,
                                       lightNumber, conn, CONFIGURE_UPDATE, HeaderLength + MessageLength * 5, message, lightId);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    获取环境信息
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"GetEnvironment": [1]} 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::getEnvironment(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    const Value& envSersors = jsonObject[GetEnvironment];
    for(SizeType i = 0; i <envSersors.Size(); i++)
    {
        int lightId = envSersors[i].GetInt();
        LOG_DEBUG << GetEnvironment << ": " << lightId;

        int devNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, devNumber, lightId, ENVIRONMENT))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength));
        MessageConstructor::inquireEnvirionment(get_pointer(message));
        function<void()> sendCb = bind(&TCPServer::sendMessageWithDT, tcpServer_,
                                       conn, devNumber, ENVIRONMENT_MSG, MinimalFrameLength,message, lightId);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    获取人群信息
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"GetHuman": [1]} 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::getHuman(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    const Value& humSersors = jsonObject[GetHuman];
    for(SizeType i = 0; i <humSersors.Size(); i++)
    {
        int lightId = humSersors[i].GetInt();
        LOG_DEBUG << GetHuman << ": " << lightId;

        int devNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, devNumber, lightId, HUMAN))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength));
        MessageConstructor::inquireHuman(get_pointer(message));
        function<void()> sendCb = bind(&TCPServer::sendMessageWithDT, tcpServer_,
                                       conn, devNumber, HUMAN_MSG, MinimalFrameLength, message, lightId);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    语音控制
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"SoundControl": [{"id": 1, "total": 4, "played": 1, "times": 3}, {"id": 2, "total": 5, "played": 1, "times": 3}]}
Output:         无
Return:         无
*************************************************/
void JsonHandler::soundControl(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    const Value& sounds = jsonObject[SoundControl];
    for(SizeType i = 0; i <sounds.Size(); i++)
    {
        const Value& sound = sounds[i];
        int id = sound["id"].GetInt();
        int total = sound["total"].GetInt();
        int played = sound["played"].GetInt();
        int times = sound["times"].GetInt();

        LOG_DEBUG << SoundControl << ": " << id;
        int devNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, devNumber, id, SOUND))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength));
        MessageConstructor::soundControl(get_pointer(message), devNumber, total, played, times);

        function<void()> sendCb = bind(&TCPServer::sendMessageWithDT, tcpServer_,
                                       conn, devNumber, SOUND_CTRL, MinimalFrameLength,message, id);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    获取辅助电源信息
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"GetPower": [1]} 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::getPower(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    const Value& powerSersors = jsonObject[GetPower];
    for(SizeType i = 0; i <powerSersors.Size(); i++)
    {
        int id = powerSersors[i].GetInt();
        LOG_DEBUG << GetPower << ": " << id;
        int devNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, devNumber, id, LIGHT))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength));
        MessageConstructor::inquirePower(get_pointer(message));
        function<void()> sendCb = bind(&TCPServer::sendMessageWithDT, tcpServer_,
                                       conn, devNumber, POWER_MSG, MinimalFrameLength,message, id);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}

/*************************************************
Description:    获取路灯配置
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"GetConfig": [{"code": 1, "id": 1}]}  1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::getConfig(const TcpConnectionPtr &jsonConn, const Document &jsonObject)
{
    const Value& configs = jsonObject[GetConfig];
    for(SizeType i = 0; i <configs.Size(); i++)
    {
        const Value& config = configs[i];
        int id = config["id"].GetInt();
        uint16_t code = config["code"].GetUint();

        LOG_DEBUG << GetConfig <<": " << id;

        int lightNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(jsonConn, conn, lightNumber, id, LIGHT))
            continue;

        shared_ptr<u_char> message((u_char*)malloc(MessageLength));
        MessageConstructor::getEEPROM(get_pointer(message), code);

        function<void()> sendCb = bind(&TCPServer::sendMessageWithDT, tcpServer_,
                                       conn, lightNumber, CONFIGURE, MinimalFrameLength,message, id);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, true);
}


/*************************************************
Description:    更新路灯配置，通过RPC查询数据库相关信息，在回调函数把配置内容发给设备
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
                {"UpdateConfig": 0x0032, "Lights": [1]} 1为灯杆Id
Output:         无
Return:         无
*************************************************/
void JsonHandler::updateConfiguration(const TcpConnectionPtr& conn, const Document& jsonObject)
{
    int eepromCode = jsonObject["UpdateConfig"].GetInt();
    const Value& lights = jsonObject["Lights"];
    MySQLRequest request;
    request.set_messagecode(eepromCode);
    for(SizeType i = 0; i < lights.Size(); i++)
    {
        int lightId = lights[i].GetInt();
        request.set_lightid(lightId);
        MySQLResponse* response = new MySQLResponse;
        MySQLRpcParam* param = new MySQLRpcParam;
        param->conn = conn;
        switch(eepromCode)
        {

            case 0x0000:
                rpcClient_->stub_.getPassword(NULL, &request, response,
                                              NewCallback(this, &JsonHandler::getEEPROM, response, param));
                break;
//            case 0x0002:
//                rpcClient_->stub_.getLastTime(NULL, &request, response,
//                                              NewCallback(this, &JsonHandler::getEEPROM, response, get_pointer(conn)));
//                break;
//            case 0x0003:
//                rpcClient_->stub_.getHarewareNumber(NULL, &request, response,
//                                              NewCallback(this, &JsonHandler::getEEPROM, response, get_pointer(conn)));
//                break;
//            case 0x0004:
//                rpcClient_->stub_.getIP(NULL, &request, response,
//                                              NewCallback(this, &JsonHandler::getEEPROM, response, get_pointer(conn)));
//                break;
//            case 0x0005:
//                rpcClient_->stub_.getDomainName(NULL, &request, response,
//                                               NewCallback(this, &JsonHandler::getEEPROM, response, get_pointer(conn)));
//                break;
            case 0x0020:
                rpcClient_->stub_.getLightConfig(NULL, &request, response,
                                                NewCallback(this, &JsonHandler::getEEPROM, response, param));
                break;
            case 0x0021:
                rpcClient_->stub_.getEnvironmentConfig(NULL, &request, response,
                                                NewCallback(this, &JsonHandler::getEEPROM, response, param));
                break;
            case 0x0022:
                rpcClient_->stub_.getCarConfig(NULL, &request, response,
                                                NewCallback(this, &JsonHandler::getEEPROM, response, param));
                break;
            case 0x0023:
                rpcClient_->stub_.getHumanConfig(NULL, &request, response,
                                                NewCallback(this, &JsonHandler::getEEPROM, response, param));
                break;
            case 0x0024:
                rpcClient_->stub_.getSoundConfig(NULL, &request, response,
                                                NewCallback(this, &JsonHandler::getEEPROM, response, param));
                break;
            case 0x0025:
                rpcClient_->stub_.getControllerSwitch(NULL, &request, response,
                                                NewCallback(this, &JsonHandler::getEEPROM, response, param));
                break;
            case 0x0030:
                rpcClient_->stub_.getPowerEnable(NULL, &request, response,
                                                NewCallback(this, &JsonHandler::getEEPROM, response, param));
                break;
            default:
                {
                    if(eepromCode >= 0x0031 && eepromCode <= 0x003F)
                        rpcClient_->stub_.getPWM(NULL, &request, response,
                                                 NewCallback(this, &JsonHandler::getEEPROM, response, param));
                    else if(eepromCode >= 0x0040 && eepromCode <= 0x0058)
                        rpcClient_->stub_.getRelay(NULL, &request, response,
                                                        NewCallback(this, &JsonHandler::getEEPROM, response, param));
                    else
                    {
                        LOG_WARN << "unknow eeprom code " << eepromCode;
                        delete param;
                        delete response;
                    }
                }
        }
    }
}


/*************************************************
Description:    更新路灯配置回调函数，把配置内容发给设备
Calls:          JsonHandler:updateConfiguration
Input:          response：RPC回复
                param：回调函数的传参
Output:         无
Return:         无
*************************************************/
void JsonHandler::getEEPROM(MySQLResponse* response, MySQLRpcParam* param)
{
    if(response->has_eeprom())
    {
        u_char* data =(u_char*) response->eeprom().c_str();
        printEEPROM(data, response->eeprom().length());

        size_t messageCount = response->eeprom().length() / MessageContentLength;

        int messageLen = messageCount * MessageLength;
        shared_ptr<u_char> sendMessage((u_char*)malloc(MessageLength));
        for(size_t i = 0; i < messageCount; i++)
            MessageConstructor::updateEEPROM(get_pointer(sendMessage) + i * MessageLength, data + i * MessageContentLength, response->messagecode());

        int lightId = response->lightid();
        int lightNumber;
        TcpConnectionPtr conn;
        if (!getConnAndDevNum(param->conn, conn, lightNumber, lightId, LIGHT))
            return;

        function<void()> sendCb = bind(&TCPServer::sendWithTimerForDC, tcpServer_,
                                       lightNumber, conn, CONFIGURE_UPDATE, messageLen + HeaderLength, sendMessage, lightId);
        conn->getLoop()->runInLoop(sendCb);


        if(param->conn->connected())
            returnJsonResult(param->conn, true);
    }
    else
    {
        if(param->conn->connected())
            returnJsonResult(param->conn, false, "configuration not found");
    }
    delete param;
}

/*************************************************
Description:    测试临时使用
Calls:          JsonHandler:
Input:          jsonConn：json的TCP连接
                jsonObjcet：json对象
Output:         无
Return:         无
*************************************************/
void JsonHandler::test(const TcpConnectionPtr& jsonConn, const Document& jsonObject)
{
    const Value& lights = jsonObject[Test];
    for(SizeType i = 0; i <lights.Size(); i++)
    {
        int lightId = lights[i].GetInt();
        LOG_INFO << Test << ": " << lightId;
        //get light number
        int lightNumber = tcpServer_->getDevNumber(lightId, LIGHT);
        if (lightNumber == -1)
        {
            LOG_WARN << "No such light connnected to TCPServer" << lightNumber;
            returnJsonResult(jsonConn, false);
            continue;
        }

        TcpConnectionPtr conn = tcpServer_->getDevConnection(lightId, LIGHT);
        if(get_pointer(conn) == NULL)
        {
            LOG_INFO << "no connection found";
            returnJsonResult(jsonConn, false);
            continue;
        }

        //updata ligth power configuration
        shared_ptr<u_char> message((u_char*)malloc(MessageLength * 5));
        u_char* messagePtr = get_pointer(message);
        for(int i = 0; i < 5; i++)
        {
            FrameMessage msg;
            msg.tableNumber = 0x04;
            msg.code = 0x0031 + i;
            msg.type = 0xF8;
            EEPROMPWMTiming timing;
            timing.port = 0x01 + i;
            timing.power = 50 + i * 10;
            timing.startTime = 270 + i *10;
            timing.lastMin = 600 + i * 10;
            memcpy(messagePtr, &msg, sizeof(msg));
            memcpy(messagePtr + 4, &timing, sizeof(timing));
            messagePtr += MessageLength;
        }
        function<void()> sendCb = bind(&TCPServer::sendWithTimerForDC, tcpServer_,
                                       lightNumber, conn, LIGHT_CTRL, HeaderLength + MessageLength * 5, message, lightId);
        conn->getLoop()->runInLoop(sendCb);
    }

    returnJsonResult(jsonConn, false);
}
