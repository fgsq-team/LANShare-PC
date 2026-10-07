//
// Created by fgsqme on 2022/3/9 0009.
//

#ifndef LANSHARE_LANSHARE_H
#define LANSHARE_LANSHARE_H

#include "Config.hpp"
#include "UDPServer.h"
#include "LHttpServer.h"
#include "TCPServer.h"
#include "TCPClient.h"
#include "Device.h"
#include "MTCPClient.h"
#include "LANShareWindow.h"
#include "DataDec.h"
#include "LFile.h"
#include "HttpServer.h"
#include "MediaIdPathDBUtil.h"
#include "TokenDBUtil.h"
#include <list>
#include <vector>
#include <map>

#include "FileSend.h"
#include "../version/four/include/FileServer.h"
#include "ThreadPool.h"
#include "DeviceManager.hpp"
#include "UdpProtocol.hpp"
#include "TcpProtocol.hpp"
#include "LegacyFileTransfer.hpp"

class FileTransfer;
class LANShareWindow;

/**
 * 接收文件请求的数据结构
 * 用于封装文件接收过程中的相关信息
 */
class AcceptFiles {
public:
    Device device;
    bool needEncData;
    std::vector<LFile *> files;
    std::unique_ptr<TCPClient> tcpClient;
    FileTransfer *fileTransfer;
    CustomDataStream *stream;
};

/**
 * LAN服务 - 主服务类
 * 负责协调各个管理器完成局域网通信任务
 * @author fgsq
 * @version 1.0
 */
class LANShare {
public:
    // 保存在线的设备
    std::map<std::string, Device> onLineDevices;
    // 自己设备信息
    std::vector<Device> mDevices;
    bool isRunning = true;
    std::unique_ptr<UDPServer> udpServer;
    std::unique_ptr<TCPServer> tcpServer;
    LANShareWindow *mainWindow;
    std::unique_ptr<LHttpServer> lhttpServer;
    std::unique_ptr<ThreadPool> tcpThreadPool;
    FileServer fileServer;
    FileSend fileSend;
    int systemVolume = 0;
    bool muted = false;

    // 拆分后的子模块
    DeviceManager deviceManager;
    UdpProtocol udpProtocol;
    TcpProtocol tcpProtocol;
    LegacyFileTransfer legacyFileTransfer;

public:
    /**
     * 构造函数
     * @param mainWindow 主窗口指针
     */
    LANShare(LANShareWindow *mainWindow);

    /**
     * 析构函数
     */
    ~LANShare();

    /**
     * 更新本机设备信息列表
     */
    void updateMDevices();

    /**
     * 获取在线设备列表
     * @return 在线设备映射表
     */
    const std::map<std::string, Device> &getOnLineDevices() const;

    /**
     * 获取本机网络设备列表
     * @return 本机网络设备列表
     */
    std::vector<Device> getMDevices() const;

    /**
     * 获取单例实例
     * @return LANShare 单例指针
     */
    static LANShare *getInstance();

    /**
     * 关闭服务
     * 通知其他设备本机下线，关闭服务器
     */
    void close();
};

#endif //LANSHARE_LANSHARE_H
