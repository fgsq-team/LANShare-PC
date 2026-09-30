//
// Created by fgsqme on 2022/3/9 0009.
//

#ifndef LANSHARE_LANSHARE_H
#define LANSHARE_LANSHARE_H

#include "Config.h"
#include "UDPServer.h"
#include "LHttpServer.h"
#include "TCPServer.h"
#include "TCPClient.h"
#include "Device.h"
#include "LANShareWindow.h"
#include "LFile.h"
#include "TokenDBUtil.h"
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class LANShareWindow;
class DataDec;

class AcceptFiles {
public:
    Device device;
    bool needEncData;
    std::vector<LFile *> files;
    std::unique_ptr<TCPClient> tcpClient;

    // ===== 分段并行传输（FS_SHARE_SEG）专用字段 =====
    bool isSeg = false;
    mlong fileSize = 0;
    QString fileName;
    std::string segId;
    int segIndex = 0;
    int segCount = 0;
    mlong segStart = 0;
    mlong segLen = 0;
    bool encData = false;
};

// 一次分段并行传输的共享状态（对应安卓端 SegCoord）。
// 多条连接各自独立跑，但它们指向同一个文件，必须协调三件事：
// 1. 落盘路径只算一次；2. 预分配全长；3. 整体进度与收尾只做一次。
class SegCoord {
public:
        std::string segId;
    mlong fileSize = 0;
    int segCount = 0;
    LFile *fileContent = nullptr;
    QString filePath;
    QString peerName;

    std::atomic<int> arrived{0};
    std::atomic<long long> received{0};
    std::atomic<bool> finished{false};
    std::atomic<bool> presented{false};
    std::atomic<bool> broken{false};
    std::thread::id creatorId;

    // 稳定判断：创建 coord 的那个线程（首段）永远是 first，可重复调用。
    // 对应安卓端 creator == Thread.currentThread()。
    bool isFirst() { return creatorId == std::this_thread::get_id(); }
    long long addReceived(long long n) { return received.fetch_add(n) + n; }
    long long receivedTotal() { return received.load(); }
    int arrivedTotal() { return arrived.load(); }
    bool arrive() { return arrived.fetch_add(1) + 1 >= segCount; }
    bool finishOnce() { bool f = false; return finished.compare_exchange_strong(f, true); }
    bool presentOnce() { bool f = false; return presented.compare_exchange_strong(f, true); }
    void markBroken() { broken.store(true); }
    bool isBroken() { return broken.load(); }
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

    int systemVolume = 0;
    bool muted = false;

public:
    void addDevice(const Device &device);

    void removeDevice(const Device &device);

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

    static void startHandleRecvFile(bool accept, const Device& device,
                                    bool needEncData, const std::vector<LFile *>& files,
                                    const std::unique_ptr<TCPClient> &tcpClient);

    // ===== 分段并行传输（FS_SHARE_SEG）=====
    // 接收侧：handleTcp 里 cmd==FS_SHARE_SEG 时进入。
    // 传 unique_ptr 引用：弹窗确认时把连接所有权转移给 AcceptFiles，自动接收时仍由 handleTcp 持有。
    static void fsSegShare(DataDec &dataDec, const Device &device,
                           std::unique_ptr<TCPClient> &tcpClient);
    static void runSeg(SegCoord *coord, int segIndex, int segCount,
                       mlong segStart, mlong segLen, TCPClient *tcpClient,
                       bool encData, const QString &outFile);
    static void finishSeg(SegCoord *coord, const QString &outFile, bool ok);
    static long long recvSegFile(TCPClient *tcpClient, int segIndex, int segCount,
                                  mlong segStart, mlong segLen, const QString &outFile,
                                  SegCoord *coord, bool dec);
    // 确认弹窗回调（isSeg 分支）
    static void startHandleRecvSeg(bool accept, AcceptFiles *af);

    // 发送侧：sendFile 里判定合格后进入
    static bool isParallelEligible(const Device &device, const std::vector<LFile *> &fileList, bool encData);
    static bool isFileParallelEligible(LFile *f);
    static void sendFileParallel(const Device &device, std::vector<LFile *> selectFiles);
    static long long sendFileParallelOne(const Device &device, LFile *file);
    static long long sendOneSeg(const Device &mDevice, const Device &device, LFile *file,
                                const std::string &segId, int idx, int segs,
                                mlong start, mlong len,
                                std::atomic<long long> *sentTotal, const QString &progressUuid,
                                std::atomic<int> *lastProgress);

    void broadcastMessage(Device *toDevice, const QString &message, bool isClip, bool shareWS);

    void noticeDeviceOnLineByIp(const QString &ip) const;

    void noticeDeviceOffLineByIp(const QString &ip) const;

    void close();
};

#endif //LANSHARE_LANSHARE_H
