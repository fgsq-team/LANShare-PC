//
// Created by fgsq on 2026/10/6.
//

#ifndef LANSHARE_DEVICEMANAGER_H
#define LANSHARE_DEVICEMANAGER_H

#include "Device.h"
#include "DataEnc.h"
#include "UDPServer.h"
#include "UDPClient.h"

class LANShare;

/**
 * 设备管理器 - 负责设备发现与管理
 * 处理设备的上线、下线、扫描等操作
 * @author fgsq
 * @version 1.0
 */
class DeviceManager {
private:
    LANShare *lanshare;

public:
    /**
     * 构造函数
     * @param lanshare LANShare 主服务指针
     */
    explicit DeviceManager(LANShare *lanshare);

    /**
     * 构造 UDP 设备数据包
     * @param device 设备信息
     * @param dataEnc 数据编码器
     */
    static void makeUdpDataEnc(const Device &device, DataEnc *dataEnc);

    /**
     * 通过 UDP 服务器发送数据
     * @param udpServer UDP 服务器指针
     * @param dataEnc 数据编码器
     * @param ip 目标 IP
     * @param port 目标端口
     */
    static void udpSend(UDPServer *udpServer, DataEnc *dataEnc, const QString &ip, int port);

    /**
     * 通过 UDP 客户端发送数据
     * @param udpClient UDP 客户端指针
     * @param dataEnc 数据编码器
     * @param ip 目标 IP
     * @param port 目标端口
     */
    static void udpSend(UDPClient *udpClient, DataEnc *dataEnc, const QString &ip, int port);

    /**
     * 添加在线设备
     * @param device 设备信息
     */
    void addDevice(const Device &device);

    /**
     * 移除在线设备
     * @param device 设备信息
     */
    void removeDevice(const Device &device);

    /**
     * 扫描网络设备
     * 循环扫描局域网内的设备，并清理超时设备
     */
    void scannDevice();

    /**
     * 通知指定 IP 的设备本机已上线
     * @param ip 目标设备 IP
     */
    void noticeDeviceOnLineByIp(const QString &ip) const;

    /**
     * 通知指定 IP 的设备本机已下线
     * @param ip 目标设备 IP
     */
    void noticeDeviceOffLineByIp(const QString &ip) const;
};

#endif //LANSHARE_DEVICEMANAGER_H
