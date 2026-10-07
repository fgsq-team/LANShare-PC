//
// Created by fgsq on 2026/10/6.
//

#include "TcpProtocol.hpp"
#include "LANShare.h"
#include "DeviceManager.hpp"
#include "LegacyFileTransfer.hpp"
#include "LHttpServer.h"
#include "TimeTools.h"
#include "ByteUtils.h"
#include "Utils.h"
#include "LException.h"
#include "DataDec.h"
#include "DataEnc.h"
#include "CustomDataStream.h"
#include "MediaIdPathDBUtil.h"
#include "mUtils.h"
#include "Config.hpp"

#define BUFF_SIZE (1024 * 1024 * 2)

/**
 * 构造函数
 */
TcpProtocol::TcpProtocol(LANShare *lanshare) : lanshare(lanshare) {}

/**
 * 创建 TCP 连接
 * 连接目标设备并发送魔数头验证
 * @return TCP 客户端指针，失败返回 nullptr
 */
std::unique_ptr<TCPClient> TcpProtocol::makeSocket(QString ip, int port) {
    auto client = std::make_unique<TCPClient>(std::move(ip), port);
    if (!client->connect()) {
        return nullptr;
    }
    auto magicBytes = std::make_unique<mbyte[]>(4);
    ByteUtils::intToBytes(MAGIC_NUM, magicBytes.get());
    try {
        client->send(magicBytes.get(), 4);
    } catch (LException &e) {
        return nullptr;
    }
    TimeTools::sleep_ms(10);
    return client;
}

/**
 * 构造 TCP 设备数据包
 * 将设备信息编码到数据编码器中
 */
void TcpProtocol::makeDataEnc(const Device &device, DataEnc *dataEnc) {
    dataEnc->putInt(device.getDevPort());
    dataEnc->putString(device.getDevIp());
    dataEnc->putString(device.getDevName());
    dataEnc->putInt(device.getDevMode());
    dataEnc->putString(Config::instance().uniqueUUid);
    dataEnc->putInt(DATA_VERSION_3);
    dataEnc->putInt(device.getBatteryLevel());
    dataEnc->putByte(device.getChargeStatus());
}

/**
 * 处理 TCP 连接
 * 根据命令类型分发处理：文件传输、消息、媒体同步等
 */
void TcpProtocol::handleTcp(std::unique_ptr<TCPClient> tcpClient) {
    auto *buffer = new mbyte[BUFF_SIZE];
    if (tcpClient->recvo(buffer, 4) != 4) return;
    const int magicNum = ByteUtils::bytesToInt(buffer, 0);
    if (magicNum != MAGIC_NUM) {
        char buff[5];
        memcpy(buff, buffer, 4);
        buff[4] = '\0';
        QString str(buff);
        str = str.toUpper();
        if (Config::instance().webService && (str.startsWith("GET") || str.startsWith("POST"))) {
            lanshare->lhttpServer->httpServer->newClient(tcpClient.get(), str);
        } else {
            tcpClient->close();
        }
        delete[] buffer;
        return;
    }
    CustomDataStream dataStream(tcpClient.get());
    int cmd = dataStream.readInt();
    if (cmd == NEW_VERSION_4) {
        Device device;
        auto deviceString = dataStream.readString();
        QJsonDocument doc = QJsonDocument::fromJson(deviceString.c_str());
        QJsonObject data = doc.object();
        device.setDevIp(data["devIP"].toString());
        device.setDevName(data["devName"].toString());
        device.setDevMode(data["devMode"].toInt());
        device.setDevPort(data["devPort"].toInt());
        device.setUniqueUUid(data["uniqueUUid"].toString());
        device.setDataVersion(data["dataVersion"].toInt());
        device.setBatteryLevel(data["batteryLevel"].toInt());
        device.setChargeStatus(data["chargeStatus"].toInt());
        if (device.getDataVersion() < DATA_VERSION_4) {
            device.setDataVersion(DATA_VERSION_4);
        }
        lanshare->fileServer.handleVersion1(device, std::move(tcpClient));
    } else {
        if (tcpClient->recvo(buffer + 4, DataEnc::headerSize() - 4) != DataEnc::headerSize() - 4) return;
        DataDec dataDec(buffer, BUFF_SIZE);
        int length = dataDec.getLength();
        if (tcpClient->recvo(buffer, DataEnc::headerSize(), length, 0) != length) return;
        dataDec.setData(buffer, DataEnc::headerSize() + length);
        // 设备端口
        const int devPort = dataDec.getInt();
        // 设备ip
        char *devIp = dataDec.getStr();
        // 设备名
        char *devName = dataDec.getStr();
        // 设备类型
        const int devMode = dataDec.getInt();
        // 设备唯一码
        const char *uniqueUUid = dataDec.getStr();
        // 协议版本
        const int dataVersion = dataDec.getInt();
        // 电量
        const int batteryLevel = dataDec.getInt();
        // 充电状态
        const mbyte chargeStatus = dataDec.getByte();
        Device device;
        device.setDevMode(devMode);
        device.setDevName(devName);
        device.setDevIp(devIp);
        device.setDevNetMask("");
        device.setDevPort(devPort);
        device.setSetTime(TimeTools::getCurrentTime());
        device.setDataVersion(dataVersion);
        device.setUniqueUUid(uniqueUUid);
        device.setBatteryLevel(batteryLevel);
        device.setChargeStatus(chargeStatus);
        lanshare->deviceManager.addDevice(device);
        DataEnc dataEnc(buffer, BUFF_SIZE);
        dataEnc.setCmd(cmd);
        if (cmd == FS_SHARE_FILE) {
            const int count = dataDec.getCount();
            const bool needEncData = dataDec.getBool();
            qDebug("file count:%d", count);
            qDebug("ip:%s:%d name:%s", devIp, devPort, devName);
            std::vector<LFile *> files;
            for (int i = 0; i < count; i++) {
                if (tcpClient->recvo(buffer, 0, DataEnc::headerSize(), 0) != DataEnc::headerSize()) return;
                length = dataDec.getLength();
                if (tcpClient->recvo(buffer, DataEnc::headerSize(), length, 0) != length) return;
                dataDec.setData(buffer, DataEnc::headerSize() + length);
                const mlong fileSize = dataDec.getLong();
                // 文件名称
                char *strFilename = dataDec.getStr();
                const int fileType = dataDec.getInt();
                const char *videoTime = dataDec.getStr();
                qDebug("fileType:%d fileName:%s  fileSize:%lld", fileType, strFilename, fileSize);
                auto *lfile = new LFile();
                lfile->setFileName(strFilename);
                lfile->setFileSize(fileSize);
                if (fileType == FILE_FOLDER) {
                    const int fileCount = dataDec.getInt();
                    lfile->setSubFileCount(fileCount);
                    lfile->setIsDirectory(true);
                } else if (fileType == FILE_IMAGE || fileType == FILE_VIEDO) {
                    const mlong mediaId = dataDec.getLongDefualt(-1);
                    lfile->setMediaId(mediaId);
                    lfile->setIsDirectory(false);
                } else {
                    lfile->setIsDirectory(false);
                }
                lfile->setMioUtil(new MTCPClient(tcpClient->getFd()));
                lfile->setUuid(Utils::getUUID());
                files.push_back(lfile);
                emit
                LANShareWindow::getInstance()->sigRecviceFile(
                    lfile,
                    lfile->getUuid(),
                    strFilename,
                    devName,
                    fileSize,
                    true,
                    !lfile->isDirectory(),
                    false
                );
                delete videoTime;
                delete strFilename;
            }
            if (Config::instance().acceptRecvFiles) {
                lanshare->legacyFileTransfer.startHandleRecvFile(true, device, needEncData, files, tcpClient);
            } else {
                auto *acceptFiles = new AcceptFiles();
                acceptFiles->device = device;
                acceptFiles->files = files;
                acceptFiles->tcpClient = std::move(tcpClient);
                emit
                LANShareWindow::getInstance()->sigRequstRecvFiles(acceptFiles);
            }
        } else if (cmd == FS_MESSAGE) {
            const char *message = dataDec.getStr();
            const QString decode = mUtils::decMessage(message, Config::instance().messageKey);
            qDebug() << "devName:" << devName << " message:" << decode;
            emit
            LANShareWindow::getInstance()->sigNewMessage(device, decode, true);
            delete[] message;
            qDebug("client close");
            tcpClient->close();
        } else if (cmd == FS_GET_NO_SYNC_MEDIA) {
            const int count = dataDec.getCount();
            DataEnc dataEnc(buffer, BUFF_SIZE);
            int syncCount = 0;
            for (int i = 0; i < count; i++) {
                const mlong mediaId = dataDec.getLong();
                MediaIdPathDBUtil mediaIdPathDbUtil;
                if (!mediaIdPathDbUtil.isIdExists(mediaId)) {
                    syncCount++;
                    dataEnc.putLong(mediaId);
                }
            }
            dataEnc.setCount(syncCount);
            tcpClient->send(dataEnc.getData(), dataEnc.getDataLen());
            qDebug("client close");
            tcpClient->close();
        }
        delete[] devIp;
        delete[] devName;
        delete[] uniqueUUid;
    }
    delete[] buffer;
}

/**
 * 创建 TCP 服务器
 * 循环接受 TCP 连接并分发到线程池处理
 */
void TcpProtocol::createTcpServer() {
    while (lanshare->isRunning) {
        std::unique_ptr<TCPClient> tcpClient = lanshare->tcpServer->accept();
        if (tcpClient == nullptr) {
            return;
        }
        lanshare->tcpThreadPool->enqueue([this, tcpClient = std::move(tcpClient)]() mutable {
            this->handleTcp(std::move(tcpClient));
        });
    }
}
