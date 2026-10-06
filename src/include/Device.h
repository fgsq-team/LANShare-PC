//
// Created by fgsqme on 2022/3/10 0010.
//

#ifndef LANSHARE_WIN_DEVICE_H
#define LANSHARE_WIN_DEVICE_H

#include <QString>
#include <ostream>
#include <QJsonObject>

#include "Type.h"

class Device {
public:
    static const int L_UNKNOW = -1;
    static const int L_ANDROID = 1;
    static const int L_WIN = 2;
    static const int L_LINUX = 3;
    static const int L_MAC = 4;
    static const int L_IOS = 5;

private:
    QString devName; // 设备名称
    QString devIP; // 设备TCP接收文件IP
    QString devNetMask; // 子网掩码
    QString devBrotIP; // 广播IP
    int devPort; // 设备TCP接收文件端口
    int devMode; // 设备代号
    mlong setTime; // 最后心跳时间
    QString uniqueUUid; // 最后心跳时间
    mlong traffic; //  接口流量
    int dataVersion = 0; // 通讯协议版本
    int batteryLevel = -1; //
    mbyte chargeStatus = -1;

public:
    const QString &getUniqueUUid() const;

    void setUniqueUUid(const QString &uniqueUUid);

    const QString &getDevName() const;

    void setDevName(const QString &devName);

    const QString &getDevIp() const;

    void setDevIp(const QString &devIp);

    const QString &getDevNetMask() const;

    void setDevNetMask(const QString &devNetMask);

    const QString &getDevBrotIp() const;

    void setDevBrotIp(const QString &devBrotIp);

    int getDevPort() const;

    void setDevPort(int devPort);

    int getDevMode() const;

    void setDevMode(int devMode);

    mlong getSetTime() const;

    void setSetTime(mlong setTime);

    int getDataVersion() const;

    void setDataVersion(int dataVersion);

    mlong getTraffic() const;

    void setTraffic(mlong traffic);

    int getBatteryLevel() const;

    void setBatteryLevel(int batteryLevel);

    mbyte getChargeStatus() const;

    void setChargeStatus(mbyte chargeStatus);

    QJsonObject toJsonObject();

    void fromJsonObject(QJsonObject jsonObject);
};


#endif //LANSHARE_WIN_DEVICE_H
