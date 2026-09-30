//
// Created by fgsqme on 2022/3/9 0009.
//

#include <QString>
#include "Device.h"
#include "Config.h"
#include <cstdio>
#include <list>
#include "NetWorldUtils.h"
#include "ByteUtils.h"
#include "BatteryUtils.h"

#if defined(PLATFORM_WINDOWS)
#include <Iphlpapi.h>
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#include <stdio.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#endif

// QString ip 转int ip
uint NetWorldUtils::strIP2intIP(const QString &strIp) {
    uchar ip[4];
    sscanf(strIp.toStdString().c_str(), "%hhu.%hhu.%hhu.%hhu", &ip[0], &ip[1], &ip[2], &ip[3]);
    int intIp = ByteUtils::bytesToInt((mbyte *) ip, 0);
    return *(uint *) &intIp;
}

// int ip 转QString ip
QString NetWorldUtils::intIP2StrIP(uint intIp) {
    QString strIp = QString::number(((intIp >> 24) & 0xFF));
    strIp += ".";
    strIp += QString::number((intIp >> 16) & 0xFF);
    strIp += ".";
    strIp += QString::number((intIp >> 8) & 0xFF);
    strIp += ".";
    strIp += QString::number((intIp) & 0xFF);
    return strIp;
}

// 通过子网掩码字符获取子网掩码长度
int NetWorldUtils::getMaskMapLength(const QString &strIp) {
    uint mask = strIP2intIP(strIp);
    int cnt = 0;
    bool flag = false;
    for (int i = 0; i < 32; i++) {
        if ((mask << i) & 0x80000000) {
            cnt++;
        } else {
            flag = true;
            break;
        }
    }
    return flag ? cnt : -1;
}

// 通过子网长度获取子网掩码
QString NetWorldUtils::getMaskMapStr(int length) {
    uint mask = 0xFFFFFFFF << (ALL_BIT - length);
    return intIP2StrIP(mask);
}

// 判断两个ip是否在同网段
bool NetWorldUtils::subNet(int sub_mask, const QString &ip_a, const QString &ip_b) {
    uint mask = 0xFFFFFFFF;
    mask = mask << (ALL_BIT - sub_mask);
    uint ipA = strIP2intIP(ip_a) & mask;
    uint ipB = strIP2intIP(ip_b) & mask;
    return ipA == ipB;
}

QString NetWorldUtils::GetBroadcastIP(const QString &ip, const QString &netmask) {
    uint intIp = strIP2intIP(ip);
    uint intmask = strIP2intIP(netmask);
    uint brodIP = (~intmask) | intIp;
    return intIP2StrIP(brodIP);
}

// 判断网卡是否正常
bool getAdapterState(int index) {
#if defined(PLATFORM_WINDOWS)
    MIB_IFROW Info;
    memset(&Info, 0, sizeof(MIB_IFROW));
    Info.dwIndex = index;
    if (GetIfEntry(&Info) != NOERROR) {
        return false;
    }
    if (Info.dwOperStatus == IF_OPER_STATUS_NON_OPERATIONAL || Info.dwOperStatus == IF_OPER_STATUS_UNREACHABLE
        || Info.dwOperStatus == IF_OPER_STATUS_DISCONNECTED || Info.dwOperStatus == IF_OPER_STATUS_CONNECTING)
        return false;
    else if (Info.dwOperStatus == IF_OPER_STATUS_OPERATIONAL || Info.dwOperStatus == IF_OPER_STATUS_CONNECTED)
        return true;
    return false;
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    return false;
#endif
}

mlong getAdapterTraffic(long index) {
#if defined(PLATFORM_WINDOWS)
    MIB_IFROW ifRow;
    ifRow.dwIndex = index;
    // 获取接口信息
    if (GetIfEntry(&ifRow) == NO_ERROR) {
        // 返回总字节数，包括接收和发送
        return ifRow.dwInOctets + ifRow.dwOutOctets;
    }
#endif
    return 0;
}

std::vector<Device> NetWorldUtils::getDevices() {
    std::vector<Device> devices;
    int batteryLevel = BatteryUtils::getBatteryPercentage();
    int chargeStatus = BatteryUtils::getBatteryStatus();
     // batteryLevel =2;
#if defined(PLATFORM_WINDOWS)
    auto pIpAdapterInfo = new IP_ADAPTER_INFO();
    PIP_ADAPTER_INFO pAdapter = nullptr;
    unsigned long stSize = sizeof(IP_ADAPTER_INFO);
    int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
    if (ERROR_BUFFER_OVERFLOW == nRel) {
        delete pIpAdapterInfo;
        pIpAdapterInfo = (PIP_ADAPTER_INFO)
                new BYTE[stSize];
        nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
    }
    if (ERROR_SUCCESS == nRel) {
        pAdapter = pIpAdapterInfo;
        while (pAdapter) {
            if (pAdapter->Type == MIB_IF_TYPE_ETHERNET || pAdapter->Type == IF_TYPE_IEEE80211) {
                IP_ADDR_STRING *pIpAddrString = &(pAdapter->IpAddressList);
                // 判断网卡是否正常
                if (getAdapterState(pAdapter->Index)) {
                    do {
                        QString ip = pIpAddrString->IpAddress.String;
                        QString mask = pIpAddrString->IpMask.String;
                        Device device;
                        device.setDevName(config.clientName);
                        device.setDevIp(ip);
                        device.setDevNetMask(mask);
                        device.setDevBrotIp(GetBroadcastIP(ip, mask));
                        device.setDevPort(config.tcpPort);
                        device.setDevMode(Device::L_WIN);
                        device.setDataVersion(DATA_VERSION);
                        device.setTraffic(getAdapterTraffic(pAdapter->Index));
                        device.setBatteryLevel(batteryLevel);
                        device.setChargeStatus(static_cast<mbyte>(chargeStatus));
                        //                        qDebug() << "getAdapterTraffic: " << device.getTraffic() << " ip:"
                        //                                 << device.getDevIp();
                        devices.push_back(device);
                        pIpAddrString = pIpAddrString->Next;
                    } while (pIpAddrString);
                }
            }
            pAdapter = pAdapter->Next;
        }
    }
    //释放内存空间
    if (pIpAdapterInfo) {
        delete[] pIpAdapterInfo;
        pIpAdapterInfo = nullptr;
    }
    // 按流量排序
    std::sort(devices.begin(), devices.end(), [](const Device &a, const Device &b) {
        return a.getTraffic() > b.getTraffic();
    });
    return devices;
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    struct ifaddrs *ifAddrStruct = NULL;
    struct ifaddrs *ifa = NULL;
    void *tmpAddrPtr = NULL;
    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            char mask[INET_ADDRSTRLEN];
            tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, ip, INET_ADDRSTRLEN);
            tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_netmask)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, mask, INET_ADDRSTRLEN);
            if (strcmp(ip, "127.0.0.1") == 0) {
                continue;
            }
            Device device;
            device.setDevName(config.clientName);
            device.setDevIp(ip);
            device.setDevNetMask(mask);
            device.setDevBrotIp(GetBroadcastIP(ip, mask));
            device.setDevPort(config.tcpPort);
            device.setDevMode(Device::L_WIN);
            device.setDataVersion(DATA_VERSION);
            device.setBatteryLevel(batteryLevel);
            device.setChargeStatus(static_cast<mbyte>(chargeStatus));
            devices.push_back(device);
        } /*else if (ifa->ifa_addr->sa_family == AF_INET6) // check it is IP6
        {
            // is a valid IP6 Address
            tmpAddrPtr = &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            qDebug("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
        }*/
    }
    if (ifAddrStruct != NULL) {
        freeifaddrs(ifAddrStruct);
    }
    return devices;
#endif
}
