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
#include "NetWorldUtils.h"
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
    if (cmd == NEW_VERSION_4 || cmd == NEW_VERSION_5) {
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
        if (cmd == NEW_VERSION_5) {
            // V5 断点续传：直接分发至独立的 FileServerV5.handleVersion，不经过 V4
            lanshare->fileServerV5.handleVersion(device, std::move(tcpClient));
        } else {
            lanshare->fileServer.handleVersion1(device, std::move(tcpClient));
        }
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
                LANShareWindow::getInstance()->sigReceiveFile(
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
                LANShareWindow::getInstance()->sigRequestRecvFiles(acceptFiles);
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
        } else if (cmd == FS_ADD_DEVICE) {
            // 扫码连接：对端(手机)扫描本机二维码后，向本机 TCP 端口发起 FS_ADD_DEVICE。
            // 扫描端设备信息已在上面通过 addDevice 入库，这里读取其 IPv6 标志，
            // 选取与扫描端同网段的本机网卡作为应答地址，回送本机设备信息，
            // 使扫描端把本机加入其设备列表（应答字段顺序须与扫描端解析一致）。
            const bool isIPv6 = dataDec.getBool();
            // 扫码添加的对端永久保留：扫码场景下对端的 UDP 心跳可能收不到（如防火墙拦截入站 UDP），
            // 若仍参与 20s 超时清理，本机很快会遗忘该设备，导致本机->对端发消息时设备已不在列表中。
            device.setCanRemove(false);
            lanshare->deviceManager.addDevice(device);
            Device localDev;
            bool found = false;
            const std::vector<Device> mDevices = lanshare->getMDevices();
            for (const Device &local: mDevices) {
                const int maskLen = NetWorldUtils::getMaskMapLength(local.getDevNetMask());
                if (maskLen > 0 && NetWorldUtils::subNet(maskLen, local.getDevIp(), device.getDevIp())) {
                    localDev = local;
                    found = true;
                    break;
                }
            }
            if (!found && !mDevices.empty()) {
                localDev = mDevices[0];
                found = true;
            }
            if (found) {
                DataEnc resp(buffer, BUFF_SIZE);
                resp.setCmd(FS_ADD_DEVICE);
                resp.putInt(localDev.getDevPort());
                resp.putString(localDev.getDevIp());
                resp.putString(localDev.getDevName());
                resp.putInt(localDev.getDevMode());
                resp.putString(Config::instance().uniqueUUid + "-" + QString::number(DATA_VERSION));
                resp.putInt(DATA_VERSION);
                resp.putBool(isIPv6);
                tcpClient->send(resp.getData(), resp.getDataLen());
                TimeTools::sleep_ms(200);
                qDebug("FS_ADD_DEVICE response to %s", device.getDevName().toUtf8().constData());
            }
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
