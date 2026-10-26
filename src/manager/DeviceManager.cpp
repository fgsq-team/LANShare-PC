//
// Created by fgsq on 2026/10/6.
//

#include "DeviceManager.hpp"
#include "LANShare.h"
#include "LHttpServer.h"
#include "NetWorldUtils.h"
#include "TimeTools.h"
#include "ByteUtils.h"
#include "StringLockManager.h"
#include "Config.hpp"
#include <unistd.h>

/**
 * 构造函数
 */
DeviceManager::DeviceManager(LANShare *lanshare) : lanshare(lanshare) {}

/**
 * 添加在线设备
 * 将设备添加到在线设备列表，并通知 WebSocket 更新设备列表
 */
void DeviceManager::addDevice(const Device &device) {
    std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
    lock.lock();
    const std::string key = device.getDevIp().toStdString() + ":" + std::to_string(device.getDevPort());
    Device toStore = device;
    // 扫码添加的设备（canRemove=false）具有粘性：后续若被普通心跳包以新 Device 覆盖，
    // 仍保留其永久在线标记，避免因未收到心跳而被超时清理。
    const auto existing = lanshare->onLineDevices.find(key);
    if (existing != lanshare->onLineDevices.end() && !existing->second.getCanRemove()) {
        toStore.setCanRemove(false);
    }
    lanshare->onLineDevices[key] = toStore;
    lock.unlock();
    LHttpServer::sendDeviceList();
}

/**
 * 移除在线设备
 * 将设备从在线设备列表中移除，并通知 WebSocket 更新设备列表
 */
void DeviceManager::removeDevice(const Device &device) {
    std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
    lock.lock();
    lanshare->onLineDevices.erase(
        device.getDevIp().toStdString() + ":" + std::to_string(device.getDevPort()));
    lock.unlock();
    LHttpServer::sendDeviceList();
}

/**
 * 构造 UDP 设备数据包
 * 将设备信息编码到数据编码器中
 */
void DeviceManager::makeUdpDataEnc(const Device &device, DataEnc *dataEnc) {
    dataEnc->putInt(device.getDevPort());
    dataEnc->putString(device.getDevIp());
    dataEnc->putString(device.getDevName());
    dataEnc->putInt(device.getDevMode());
    dataEnc->putString(Config::instance().uniqueUUid + "-" + QString::number(DATA_VERSION));
    dataEnc->putInt(DATA_VERSION_3);
    dataEnc->putInt(device.getBatteryLevel());
    dataEnc->putByte(device.getChargeStatus());
}

/**
 * 通过 UDP 服务器发送数据
 * 添加魔数头并发送数据
 */
void DeviceManager::udpSend(UDPServer *udpServer, DataEnc *dataEnc, const QString &ip, int port) {
    mbyte *data = dataEnc->encData();
    auto *newBytes = new mbyte[2048 + 4];
    ByteUtils::intToBytes(MAGIC_NUM, newBytes);
    int dataLen = dataEnc->getDataLen();
    memcpy(newBytes + 4, data, dataLen);
    udpServer->sendto(ip, port, newBytes, dataLen + 4);
    delete[] newBytes;
}

/**
 * 通过 UDP 客户端发送数据
 * 添加魔数头并发送数据
 */
void DeviceManager::udpSend(UDPClient *udpClient, DataEnc *dataEnc, const QString &ip, int port) {
    mbyte *data = dataEnc->encData();
    auto *newBytes = new mbyte[2048 + 4];
    ByteUtils::intToBytes(MAGIC_NUM, newBytes);
    int dataLen = dataEnc->getDataLen();
    memcpy(newBytes + 4, data, dataLen);
    udpClient->sendto(ip, port, newBytes, dataLen + 4);
    delete[] newBytes;
}

/**
 * 扫描网络设备
 * 循环扫描局域网内的设备，并清理超时设备（20秒无响应）
 */
void DeviceManager::scannDevice() {
    mbyte buffer[2048];
    while (lanshare->isRunning) {
        lanshare->updateMDevices();
        std::vector<Device> devices = lanshare->getMDevices();
        std::vector<Device>::iterator p1;
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            DataEnc dataEnc(buffer, 2048);
            makeUdpDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_GET_DEVICES);
            UDPClient udpClient(p1->getDevIp());
            udpSend(&udpClient, &dataEnc, p1->getDevBrotIp(), Config::instance().udpPort);
            udpClient.close();
            std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
            lock.lock();
            std::map<std::string, Device>::iterator iter;
            for (iter = lanshare->onLineDevices.begin(); iter != lanshare->onLineDevices.end();) {
                // 扫码添加的设备永久保留，不参与心跳超时清理（与手机端 canRemove 语义一致）
                if (!iter->second.getCanRemove()) {
                    ++iter;
                    continue;
                }
                mlong devTime = iter->second.getSetTime();
                mlong currentTime = TimeTools::getCurrentTime();
                mlong timeOut = currentTime - devTime;
                if (timeOut > (1000 * 20)) {
                    lanshare->onLineDevices.erase(iter++);
                } else {
                    ++iter;
                }
            }
            lock.unlock();
        }
        // 可中断等待：正常每 5 秒扫描一次，close() 时立即返回并退出循环，不拖慢退出
        if (!lanshare->waitForStop(5000)) break;
    }
}

/**
 * 通知指定 IP 的设备本机已上线
 * 向目标 IP 发送上线通知
 */
void DeviceManager::noticeDeviceOnLineByIp(const QString &ip) const {
    std::vector<Device> devices = lanshare->getMDevices();
    std::vector<Device>::iterator p1;
    for (p1 = devices.begin(); p1 != devices.end(); p1++) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(), ip)) {
            auto *bytes = new mbyte[2048];
            DataEnc dataEnc(bytes, 2048);
            makeUdpDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_SET_DEVICES);
            UDPClient udpClient(p1->getDevIp());
            udpSend(&udpClient, &dataEnc, ip, Config::instance().udpPort);
            delete[] bytes;
            break;
        }
    }
}

/**
 * 通知指定 IP 的设备本机已下线
 * 向目标 IP 发送下线通知
 */
void DeviceManager::noticeDeviceOffLineByIp(const QString &ip) const {
    std::vector<Device> devices = lanshare->getMDevices();
    std::vector<Device>::iterator p1;
    for (p1 = devices.begin(); p1 != devices.end(); p1++) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(), ip)) {
            auto *bytes = new mbyte[2048];
            DataEnc dataEnc(bytes, 2048);
            makeUdpDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_DEVICE_OFF_LINE);
            UDPClient udpClient(p1->getDevIp());
            udpSend(&udpClient, &dataEnc, ip, Config::instance().udpPort);
            delete[] bytes;
            break;
        }
    }
}
