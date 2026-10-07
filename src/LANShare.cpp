#include <QDir>
#include "LANShare.h"
#include "NetWorldUtils.h"
#include "TimeTools.h"
#include "StringLockManager.h"
#include <vector>

LANShare *instance = nullptr;

/**
 * 构造函数
 * 初始化各子模块，启动 UDP/TCP 服务器，通知其他设备本机已上线
 */
LANShare::LANShare(LANShareWindow *mainWindow)
    : mainWindow(mainWindow),
      deviceManager(this),
      udpProtocol(this),
      tcpProtocol(this),
      legacyFileTransfer(this) {
    instance = this;
    updateMDevices();
    mainWindow->updateWebServiceIp();
    udpServer = std::make_unique<UDPServer>(Config::instance().udpPort);
    tcpServer = std::make_unique<TCPServer>(Config::instance().tcpPort);
    tcpServer->bind();
    lhttpServer = std::make_unique<LHttpServer>(this);
    tcpThreadPool = std::make_unique<ThreadPool>(20);
    // 通知设备我已上线
    for (const auto &device: getMDevices()) {
        deviceManager.noticeDeviceOnLineByIp(device.getDevBrotIp());
    }
}

/**
 * 析构函数
 * 清空单例指针
 */
LANShare::~LANShare() {
    instance = nullptr;
}

/**
 * 更新本机设备信息列表
 * 获取本机所有网络设备信息
 */
void LANShare::updateMDevices() {
    std::mutex &lock = StringLockManager::getStringLock("mDevicesMutex");
    lock.lock();
    mDevices = NetWorldUtils::getDevices();
    lock.unlock();
}

/**
 * 获取本机网络设备列表
 * @return 本机网络设备列表的副本
 */
std::vector<Device> LANShare::getMDevices() const {
    std::mutex &lock = StringLockManager::getStringLock("mDevicesMutex");
    lock.lock();
    std::vector<Device> devices = mDevices;
    lock.unlock();
    return devices;
}

/**
 * 获取单例实例
 * @return LANShare 单例指针
 */
LANShare *LANShare::getInstance() {
    return instance;
}

/**
 * 获取在线设备列表
 * @return 在线设备映射表
 */
const std::map<std::string, Device> &LANShare::getOnLineDevices() const {
    return onLineDevices;
}

/**
 * 关闭服务
 * 通知其他设备本机下线，关闭 UDP/TCP 服务器和线程池
 */
void LANShare::close() {
    // 通知设备我已下线
    for (const auto &device: getMDevices()) {
        deviceManager.noticeDeviceOffLineByIp(device.getDevBrotIp());
    }
    isRunning = false;
    int result = udpServer->close();
    int result2 = tcpServer->close();
    qDebug() << "LANShare Server is closed" << result << result2;
}
