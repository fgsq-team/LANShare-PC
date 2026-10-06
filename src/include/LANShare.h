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

class FileTransfer;
class LANShareWindow;

class AcceptFiles {
public:
    Device device;
    bool needEncData;
    std::vector<LFile *> files;
    std::unique_ptr<TCPClient> tcpClient;
    FileTransfer *fileTransfer;
    CustomDataStream *stream;
};

class LANShare {
public:
    // 保存在线的设备
    std::map<std::string, Device> onLineDevices;
    // 自己设备信息
    std::vector<Device> mDevices;
    bool isRun = true;
    std::unique_ptr<UDPServer> udpServer;
    std::unique_ptr<TCPServer> tcpServer;
    LANShareWindow *mainWindow;
    std::unique_ptr<LHttpServer> lhttpServer;
    std::unique_ptr<ThreadPool> tcpThreadPool;
    FileServer fileServer;
    FileSend fileSend;
    int systemVolume = 0;
    bool muted = false;

public:
    LANShare(LANShareWindow *mainWindow);

    ~LANShare();

    void updateMDevices();

    const std::map<std::string, Device> &getOnLineDevices() const;

public:
    std::vector<Device> getMDevices() const;

    static LANShare *getInstance();

    static void handleUdp();

    static void createTcpServer();

    static void scannDevice();

    static void handleTcp(std::unique_ptr<TCPClient> tcpClient);

    static void sendFile(const Device &device, std::vector<LFile *> selectFiles, int count);

   static void startNewVersionHandleRecvFile(Device fromDevice, FileTransfer *fileTransfer, std::vector<LFile *> files,
                                       CustomDataStream *stream, boolean encData, boolean isAgree);


    static void startHandleRecvFile(bool accept, const Device &device,
                                    bool needEncData, const std::vector<LFile *> &files,
                                    const std::unique_ptr<TCPClient> &tcpClient);

    void broadcastMessage(Device *toDevice, const QString &message, bool isClip, bool shareWS);

    void noticeDeviceOnLineByIp(const QString &ip) const;

    void noticeDeviceOffLineByIp(const QString &ip) const;

    void close();
};

#endif //LANSHARE_LANSHARE_H
