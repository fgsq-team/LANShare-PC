//
// Created by fgsqme on 2022/3/10 0010.
//

#include "Device.h"



const QString &Device::getDevName() const {
    return devName;
}

void Device::setDevName(const QString &devName) {
    Device::devName = devName;
}

const QString &Device::getDevIp() const {
    return devIP;
}

void Device::setDevIp(const QString &devIp) {
    devIP = devIp;
}

const QString &Device::getDevNetMask() const {
    return devNetMask;
}

void Device::setDevNetMask(const QString &devNetMask) {
    Device::devNetMask = devNetMask;
}

const QString &Device::getDevBrotIp() const {
    return devBrotIP;
}

void Device::setDevBrotIp(const QString &devBrotIp) {
    devBrotIP = devBrotIp;
}

int Device::getDevPort() const {
    return devPort;
}

void Device::setDevPort(int devPort) {
    Device::devPort = devPort;
}

int Device::getDevMode() const {
    return devMode;
}

void Device::setDevMode(int devMode) {
    Device::devMode = devMode;
}

mlong Device::getSetTime() const {
    return setTime;
}

void Device::setSetTime(mlong setTime) {
    Device::setTime = setTime;
}

int Device::getDataVersion() const {
    return dataVersion;
}

void Device::setDataVersion(int dataVersion) {
    Device::dataVersion = dataVersion;
}

const QString &Device::getUniqueUUid() const {
    return uniqueUUid;
}

void Device::setUniqueUUid(const QString &uniqueUUid) {
    Device::uniqueUUid = uniqueUUid;
}

mlong Device::getTraffic() const {
    return traffic;
}

void Device::setTraffic(mlong traffic) {
    Device::traffic = traffic;
}

int Device::getBatteryLevel() const {
    return batteryLevel;
}

void Device::setBatteryLevel(int batteryLevel) {
    Device::batteryLevel = batteryLevel;
}

mbyte Device::getChargeStatus() const {
    return chargeStatus;
}

void Device::setChargeStatus(mbyte chargeStatus) {
    Device::chargeStatus = chargeStatus;
}

LWebSocketServer * Device::getWebSocketServer() const {
    return webSocketServer;
}

void Device::setWebSocketServer(LWebSocketServer *webSocketServer) {
    this->webSocketServer = webSocketServer;
}

void Device::setCanRemove(bool canRemove) {
    this->canRemove = canRemove;
}

bool Device::getCanRemove() const {
    return canRemove;
}
