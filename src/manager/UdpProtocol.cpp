//
// Created by fgsq on 2026/10/6.
//

#include "UdpProtocol.hpp"
#include "LANShare.h"
#include "DeviceManager.hpp"
#include "TcpProtocol.hpp"
#include "LHttpServer.h"
#include "NetWorldUtils.h"
#include "TimeTools.h"
#include "ByteUtils.h"
#include "CodeUtils.h"
#include "Utils.h"
#include "StringLockManager.h"
#include "Config.hpp"
#include "mUtils.h"
#include "DataDec.h"

/**
 * 构造函数
 */
UdpProtocol::UdpProtocol(LANShare *lanshare) : lanshare(lanshare) {}

/**
 * 处理 UDP 消息接收
 * 循环监听并处理各类 UDP 消息：设备发现、消息、剪贴板、媒体控制等
 */
void UdpProtocol::handleUdp() {
    auto *buffer = new mbyte[4096];
    sockaddr_in clientAddr{};
flag:
    while (lanshare->isRun) {
        int len = lanshare->udpServer->recv(&clientAddr, buffer, 4096);
        if (len <= 0) {
            qDebug("runRecive recv len is <= 0");
            break;
        }
        int magicNum = ByteUtils::bytesToInt(buffer, 0);
        if (magicNum != MAGIC_NUM) {
            continue;
        }
        DataDec dataDec(buffer + 4, len - 4);
        dataDec.decAllData();
        // 设备端口
        int devPort = dataDec.getInt();
        // 设备ip
        char *devIp = dataDec.getStr();
        // 设备名
        char *devName = dataDec.getStr();
        // 设备类型
        int devMode = dataDec.getInt();
        // 设备唯一码
        char *uniqueUUid = dataDec.getStr();
        int dataVersion = dataDec.getInt();
        int batteryLevel = dataDec.getInt();
        mbyte chargeStatus = dataDec.getByte();
        QString uniqueUUidStr = uniqueUUid;
        if (uniqueUUidStr.contains("-")) {
            auto split = uniqueUUidStr.split("-");
            dataVersion = split[1].toInt();
            uniqueUUidStr = split[0].toUtf8().data();
        }
        // 判断是否为自己发送的指令
        std::vector<Device> devices = lanshare->getMDevices();
        std::vector<Device>::iterator p1;
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            if (strcmp(devIp, p1->getDevIp().toUtf8().data()) == 0) goto flag;
        }
        Device device;
        device.setDevMode(devMode);
        device.setDevName(devName);
        device.setDevIp(devIp);
        device.setDevNetMask("");
        device.setDevPort(devPort);
        device.setSetTime(TimeTools::getCurrentTime());
        device.setDataVersion(dataVersion);
        device.setUniqueUUid(uniqueUUidStr);
        device.setBatteryLevel(batteryLevel);
        device.setChargeStatus(chargeStatus);
        lanshare->deviceManager.addDevice(device);
        int cmd = dataDec.getCmd();
        if (cmd == UDP_SET_DEVICES) {
            // 添加或覆盖设备
        } else if (cmd == UDP_GET_DEVICES) {
            //获取设备命令
            lanshare->deviceManager.noticeDeviceOnLineByIp(devIp);
        } else if (cmd == UDP_DEVICE_OFF_LINE) {
            //设备下线
            lanshare->deviceManager.removeDevice(device);
        } else if (cmd == UDP_MESSAGE) {
            // 消息
            char *message = dataDec.getStr();
            QString decode = mUtils::decMessage(message, Config::instance().messageKey);
            qDebug() << "devName:" << devName << " message:" << decode;
            emit
            LANShareWindow::getInstance()->sigNewMessage(device, decode, true);
            lanshare->lhttpServer->sendWebSocketMessage(decode, devName, "", 0, devMode, "", true, false);
            delete[] message;
        } else if (cmd == UDP_MESSAGE_TO_CLIPBOARD) {
            // 消息写入剪切板
            char *message = dataDec.getStr();
            QString decode = mUtils::decMessage(message, Config::instance().messageKey);
            qDebug() << "devName:" << devName << " clip message:" << decode;
            emit
            LANShareWindow::getInstance()->sigNewMessage(device, decode, true);
            emit
            LANShareWindow::getInstance()->sigCopyText(decode);
            delete[] message;
        } else if (cmd == UDP_SEND_MEDIA_MUTE) {
            // 暂停
            if (!Config::instance().receivceMute) {
                continue;
            }
            qDebug("静音");
#if defined(PLATFORM_WINDOWS)
            try {
                if (!lanshare->muted) {
                    lanshare->systemVolume = Utils::volume();
                    qDebug() << "volume:" << lanshare->systemVolume;
                    Utils::setVolum(0);
                    TimeTools::sleep_ms(500);
                    lanshare->muted = true;
                }
            } catch (const std::exception &e) {
            }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
            lanshare->systemVolume = Utils::volume();
            qDebug() << "volume:" << lanshare->systemVolume;
            Utils::setVolum(0);
            lanshare->muted = true;
#endif
        } else if (cmd == UDP_SEND_MEDIA_RESTORE) {
            if (!Config::instance().receivceMute) {
                continue;
            }
            qDebug("恢复音量");
#if defined(PLATFORM_WINDOWS)
            try {
                Utils::setVolum(lanshare->systemVolume);
                lanshare->muted = false;
            } catch (const std::exception &e) {
            }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#endif
        }
        delete[] devIp;
        delete[] devName;
        delete[] uniqueUUid;
    }
}

/**
 * 广播消息到指定设备或所有在线设备
 * 根据消息长度选择 UDP 广播或 TCP 单播方式发送
 */
void UdpProtocol::broadcastMessage(Device *toDevice, const QString &message, bool isClip, bool shareWS) {
    int strlen = CodeUtils::getUtf8StrLen(message.toUtf8().data());
    if (strlen <= 0) {
        return;
    }
    if (shareWS) {
        lanshare->lhttpServer->sendWebSocketMessage(message, Config::instance().clientName, "", 0, 1, "", false, false);
    }
    QByteArray encMessage = mUtils::encMessage(message, Config::instance().messageKey);
    int buffLen = 1024 + encMessage.size();
    auto *buffer = new mbyte[buffLen];
    if (strlen > 700) {
        std::vector<Device> devices = lanshare->getMDevices();
        std::vector<Device>::iterator p1;
        if (toDevice == nullptr) {
            std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
            lock.lock();
            std::map<std::string, Device>::iterator iter;
            for (iter = lanshare->onLineDevices.begin(); iter != lanshare->onLineDevices.end(); iter++) {
                for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                    if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                              iter->second.getDevIp())) {
                        DataEnc dataEnc(buffer, buffLen);
                        DeviceManager::makeUdpDataEnc(*p1, &dataEnc);
                        dataEnc.setCmd(FS_MESSAGE);
                        dataEnc.putString(encMessage);
                        std::unique_ptr<TCPClient> socket = TcpProtocol::makeSocket(iter->second.getDevIp(),
                                                                       iter->second.getDevPort());
                        if (socket == nullptr) {
                            break;
                        }
                        socket->send(dataEnc.getData(), dataEnc.getDataLen());
                        break;
                    }
                }
            }
            lock.unlock();
        } else {
            for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                          toDevice->getDevIp())) {
                    DataEnc dataEnc(buffer, buffLen);
                    dataEnc.setCmd(FS_MESSAGE);
                    DeviceManager::makeUdpDataEnc(*p1, &dataEnc);
                    dataEnc.putString(encMessage);
                    std::unique_ptr<TCPClient> socket = TcpProtocol::makeSocket(toDevice->getDevIp(), toDevice->getDevPort());
                    if (socket == nullptr) {
                        break;
                    }
                    socket->send(dataEnc.getData(), dataEnc.getDataLen());
                    break;
                }
            }
        }
        delete[] buffer;
        return;
    }
    std::vector<Device> devices = lanshare->getMDevices();
    std::vector<Device>::iterator p1;
    if (toDevice == nullptr) {
        std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
        lock.lock();
        std::map<std::string, Device>::iterator iter;
        for (iter = lanshare->onLineDevices.begin(); iter != lanshare->onLineDevices.end(); iter++) {
            for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                          iter->second.getDevIp())) {
                    DataEnc dataEnc(buffer, buffLen);
                    DeviceManager::makeUdpDataEnc(*p1, &dataEnc);
                    dataEnc.setCmd(isClip ? UDP_MESSAGE_TO_CLIPBOARD : UDP_MESSAGE);
                    dataEnc.putString(encMessage);
                    DeviceManager::udpSend(lanshare->udpServer.get(), &dataEnc, iter->second.getDevIp(), Config::instance().udpPort);
                    break;
                }
            }
        }
        lock.unlock();
    } else {
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                      toDevice->getDevIp())) {
                DataEnc dataEnc(buffer, buffLen);
                dataEnc.setCmd(isClip ? UDP_MESSAGE_TO_CLIPBOARD : UDP_MESSAGE);
                DeviceManager::makeUdpDataEnc(*p1, &dataEnc);
                dataEnc.putString(encMessage);
                DeviceManager::udpSend(lanshare->udpServer.get(), &dataEnc, toDevice->getDevIp(), Config::instance().udpPort);
                break;
            }
        }
    }
    delete[] buffer;
}
