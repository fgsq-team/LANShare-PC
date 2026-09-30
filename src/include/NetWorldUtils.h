//
// Created by fgsqme on 2022/3/9 0009.
//

#ifndef LANSHARE_NETWORLDTOOLS_H
#define LANSHARE_NETWORLDTOOLS_H

#include "Device.h"
#include <list>

#define ALL_BIT 32 /* ip address have 32 bits */


class NetWorldUtils {
public:

    // 获取本机IP列表
    static std::vector<Device> getDevices();

    // 计算广播IP
    static QString GetBroadcastIP(const QString &ip, const QString &netmask);

    static QString getMaskMapStr(int length);

    static uint strIP2intIP(const QString &strIp);

    static QString intIP2StrIP(uint intIp);

    static int getMaskMapLength(const QString &strIp);

    static bool subNet(int sub_mask, const QString &ip_a, const QString &ip_b);
};


#endif
