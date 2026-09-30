#include <list>
#include <thread>
#include <unistd.h>
#include <QDir>
#include <QUuid>
#include <QApplication>
#include "DataDec.h"
#include "DataEnc.h"
#include "UDPClient.h"
#include "TCPServer.h"
#include "LANShare.h"
#include "IOUtils.h"
#include "NetWorldUtils.h"
#include "TokenDBUtil.h"
#include "TimeTools.h"
#include "CodeUtils.h"
#include "Utils.h"
#include "LException.h"
#include "ByteUtils.h"

#include "LHttpServer.h"
#include "ByteArrayIOUtils.h"
#include "StringLockManager.h"
#include <utility>
#include <vector>

#include "BatteryUtils.h"
#include "MediaIdPathDBUtil.h"

LANShare *instance = nullptr;

#define BUFF_SIZE (1024 * 1024 * 2)

void LANShare::addDevice(const Device &device) {
    std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
    lock.lock();
    onLineDevices[device.getDevIp().toStdString() + ":" +
                  std::to_string(device.getDevPort())] = device;
    lock.unlock();
    LHttpServer::sendDeviceList();
}

void LANShare::removeDevice(const Device &device) {
    std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
    lock.lock();
    onLineDevices.erase(
        device.getDevIp().toStdString() + ":" + std::to_string(device.getDevPort()));
    lock.unlock();
    LHttpServer::sendDeviceList();
}

LANShare::LANShare(LANShareWindow *mainWindow) : mainWindow(mainWindow) {
    // qDebug() << "Battery: " <<BatteryUtils::getBatteryPercentage();
    instance = this;
    updateMDevices();
    mainWindow->updateWebServiceIp();
    udpServer = std::make_unique<UDPServer>(config.udpPort);
    tcpServer = std::make_unique<TCPServer>(config.tcpPort);
    tcpServer->bind();
    lhttpServer = std::make_unique<LHttpServer>(this);
    // 通知设备我已上线
    for (const auto &device: getMDevices()) {
        noticeDeviceOnLineByIp(device.getDevBrotIp());
    }
}

LANShare::~LANShare() {
    instance = nullptr;
}

void LANShare::updateMDevices() {
    std::mutex &lock = StringLockManager::getStringLock("mDevicesMutex");
    lock.lock();
    mDevices = NetWorldUtils::getDevices();
    lock.unlock();
}

void encData(mbyte *buffer, int len, int off, mlong index) {
    int j = 0;
    for (int i = off; i < len + off; i++) {
        int v = (buffer[i] - 1) ^ (int) ((index + j) & 0xFF);
        buffer[i] = (mbyte) v;
        j++;
    }
}

void decData(mbyte *buffer, int len, int off, mlong index) {
    int j = 0;
    for (int i = off; i < len + off; i++) {
        int v = (buffer[i] ^ (int) ((index + j) & 0xFF)) + 1;
        buffer[i] = (mbyte) v;
        j++;
    }
}

std::unique_ptr<TCPClient> makeSocket(QString ip, int port) {
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

void makeDataEnc(const Device &device, DataEnc *dataEnc) {
    dataEnc->putInt(device.getDevPort());
    dataEnc->putString(device.getDevIp());
    dataEnc->putString(device.getDevName());
    dataEnc->putInt(device.getDevMode());
    dataEnc->putString(config.uniqueUUid);
    dataEnc->putInt(DATA_VERSION);
    dataEnc->putInt(device.getBatteryLevel());
    dataEnc->putByte(device.getChargeStatus());
}

void udpSend(UDPServer *udpServer, DataEnc *dataEnc, const QString &ip, int port) {
    mbyte *data = dataEnc->encData();
    auto *newBytes = new mbyte[2048 + 4];
    ByteUtils::intToBytes(MAGIC_NUM, newBytes);
    int dataLen = dataEnc->getDataLen();
    memcpy(newBytes + 4, data, dataLen);
    udpServer->sendto(ip, port, newBytes, dataLen + 4);
    delete[] newBytes;
}

void udpSend(UDPClient *udpClient, DataEnc *dataEnc, const QString &ip, int port) {
    mbyte *data = dataEnc->encData();
    auto *newBytes = new mbyte[2048 + 4];
    ByteUtils::intToBytes(MAGIC_NUM, newBytes);
    int dataLen = dataEnc->getDataLen();
    memcpy(newBytes + 4, data, dataLen);
    udpClient->sendto(ip, port, newBytes, dataLen + 4);
    delete[] newBytes;
}

mlong baseSend(LFile *file, LFile *f, mlong size, mlong totalFileSize, DataEnc &dataEnc) {
    qDebug("lFile：%llX", file);
    mlong totalSend = size;
    mlong thatSend = 0;
    IOInter *ioUtils;
    if (f->getType() == LFile::BYTEARRAY) {
        ioUtils = new ByteArrayIOUtils(f->getByteArray());
    } else if (f->getType() == LFile::FILE) {
        ioUtils = new IOUtils(f->path, QFile::ReadOnly);
    } else if (f->getType() == LFile::STREAM) {
        ioUtils = f->getIoInter();
    } else {
        qDebug("发送的文件类型不支持");
        return -1;
    }
    qDebug("发送文件: %s", f->fileName.toStdString().c_str());
    try {
        qDebug("文件大小: %lld", f->fileSize);
        dataEnc.reset();
        dataEnc.setByteCmd(FS_DATA);
        int ten;
        int p = 0;
        mbyte read = 0;
        while ((ten = ioUtils->read(dataEnc.getBuffer() + DataEnc::headerSize(), BUFF_SIZE - DataEnc::headerSize())) >
               0) {
            dataEnc.setDataIndex(ten);
            MTCPClient *mtcpClient = file->mioUtil;
            if (mtcpClient->send(dataEnc.getData(), dataEnc.getDataLen()) != dataEnc.getDataLen()) {
                thatSend = -3;
                qDebug("send error");
                break;
            }
            mtcpClient->recvo(&read, 1);
            if (read == FS_BREAK) {
                thatSend = 0;
                break;
            }
            if (!file->isNextStep()) {
                thatSend = -3;
                break;
            }
            totalSend += ten;
            thatSend += ten;
            int progress = (int) ((double) totalSend * 100.0 / (double) totalFileSize);
            if (progress != p) {
                qDebug("progress: %d", progress);
                p = progress;
                emit
                LANShareWindow::getInstance()->sigRecviceFileProgress(f->getUuid(), progress);
            }
        }
    } catch (LException &e) {
        qDebug() << e.what().c_str();
        thatSend = 0;
    }
    ioUtils->close();
    if (f->getType() != LFile::STREAM) {
        delete ioUtils;
    }
    qDebug("文件大小:%lld 已发送文件大小:%lld", f->getFileSize(), thatSend);
    dataEnc.reset();
    if (thatSend != f->fileSize) {
        dataEnc.setByteCmd(FS_CLOSE);
        qDebug("文件发送失败");
    } else {
        dataEnc.setByteCmd(FS_END);
        qDebug("文件发送成功");
    }
    try {
        file->mioUtil->TCPClient::send(dataEnc.getData(), dataEnc.getDataLen());
    } catch (LException &e) {
    }
    return thatSend;
}

mlong baseSendEnc(LFile *file, LFile *f, mlong size, mlong totalFileSize, DataEnc &dataEnc) {
    mlong totalSend = size;
    mlong thatSend = 0;
    IOInter *ioUtils;
    if (f->getType() == LFile::BYTEARRAY) {
        ioUtils = new ByteArrayIOUtils(f->getByteArray());
    } else if (f->getType() == LFile::FILE) {
        ioUtils = new IOUtils(f->path, QFile::ReadOnly);
    } else if (f->getType() == LFile::STREAM) {
        ioUtils = f->getIoInter();
    } else {
        qDebug("发送的文件类型不支持");
        return -1;
    }
    qDebug("发送文件: %s", f->fileName.toStdString().c_str());
    try {
        qDebug("文件大小: %lld", f->fileSize);
        dataEnc.reset();
        dataEnc.setByteCmd(FS_DATA);
        int ten;
        int p = 0;
        mbyte read = 0;
        while ((ten = ioUtils->read(dataEnc.getBuffer() + DataEnc::headerSize(), BUFF_SIZE - DataEnc::headerSize())) >
               0) {
            dataEnc.setDataIndex(ten);
            encData(dataEnc.getBuffer(), ten, DataEnc::headerSize(), thatSend);
            MTCPClient *mtcpClient = file->mioUtil;
            if (mtcpClient->send(dataEnc.getData(), dataEnc.getDataLen()) != dataEnc.getDataLen()) {
                thatSend = -3;
                qDebug("send error");
                break;
            }
            mtcpClient->recvo(&read, 1);
            if (read == FS_BREAK) {
                thatSend = 0;
                break;
            }
            if (!file->isNextStep()) {
                thatSend = -3;
                break;
            }
            totalSend += ten;
            thatSend += ten;
            int progress = (int) ((double) totalSend * 100.0 / (double) totalFileSize);
            if (progress != p) {
                qDebug("progress: %d", progress);
                p = progress;
                emit
                LANShareWindow::getInstance()->sigRecviceFileProgress(f->getUuid(), progress);
            }
        }
        ioUtils->close();
    } catch (LException &e) {
        qDebug() << e.what().c_str();
        thatSend = 0;
    }
    if (file->getType() != LFile::STREAM) {
        delete ioUtils;
    }
    qDebug("文件大小:%lld 已发送文件大小:%lld", file->getFileSize(), thatSend);
    dataEnc.reset();
    if (thatSend != file->fileSize) {
        dataEnc.setByteCmd(FS_CLOSE);
        qDebug("文件发送失败");
    } else {
        dataEnc.setByteCmd(FS_END);
        qDebug("文件发送成功");
    }
    try {
        file->mioUtil->TCPClient::send(dataEnc.getData(), dataEnc.getDataLen());
    } catch (LException &e) {
    }
    return thatSend;
}

mlong baseRecv(const LFile *mfile,
               IOUtils &fileIO,
               DataDec &dataDec, mlong mTotalRecv) {
    mlong totalRecv = mTotalRecv;
    mlong thatTotal = 0;
    int p = 0;
    MTCPClient *mioUtil = mfile->mioUtil;
    mbyte *buffer = dataDec.getBuffer();
    while (true) {
        try {
            if (mioUtil->recvo(buffer, 0, DataEnc::headerSize(), 0) != DataEnc::headerSize()) break;
            dataDec.setData(buffer, BUFF_SIZE);
            mbyte rCmd = dataDec.getByteCmd();
            if (rCmd == FS_DATA) {
                // 数据包长度
                int length = dataDec.getLength();
                if (mioUtil->recvo(buffer, DataEnc::headerSize(), length, 0) != length) {
                    qDebug("recvo error");
                    break;
                }
                int rel = fileIO.write((char *) buffer, DataEnc::headerSize(), length);
                totalRecv += rel;
                thatTotal += rel;
                int progress = (int) ((double) totalRecv * 100.0 / (double) mfile->fileSize);
                if (p != progress) {
                    qDebug("progress:%d", progress);
                    p = progress;
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileProgress(mfile->getUuid(), progress);
                }
                mbyte send;
                if (mfile->isNextStep()) {
                    send = FS_NEXT;
                } else {
                    send = FS_BREAK;
                }
                mioUtil->send(&send, 1);
            } else if (rCmd == FS_END) {
                // 传输完毕
                break;
            } else /*if (rCmd == FS_CLOSE)*/ {
                // 关闭传输
                /*totalRecv = 0;
                break;*/
                return -3;
            }
        } catch (LException &e) {
            thatTotal = 0;
            //           qDebug();
        }
    }
    fileIO.close();
    return thatTotal;
}

mlong baseRecvDec(const LFile *mfile,
                  IOUtils &fileIO,
                  DataDec &dataDec, mlong mTotalRecv) {
    mlong totalRecv = mTotalRecv;
    mlong thatTotal = 0;
    int p = 0;
    MTCPClient *mioUtil = mfile->mioUtil;
    mbyte *buffer = dataDec.getBuffer();
    while (true) {
        try {
            if (mioUtil->recvo(buffer, 0, DataEnc::headerSize(), 0) != DataEnc::headerSize()) break;
            dataDec.setData(buffer, BUFF_SIZE);
            mbyte rCmd = dataDec.getByteCmd();
            if (rCmd == FS_DATA) {
                // 数据包长度
                int length = dataDec.getLength();
                if (mioUtil->recvo(buffer, DataEnc::headerSize(), length, 0) != length) {
                    qDebug("recvo error");
                    break;
                }
                decData(buffer, length, DataEnc::headerSize(), thatTotal);
                int rel = fileIO.write((char *) buffer, DataEnc::headerSize(), length);
                totalRecv += rel;
                thatTotal += rel;
                int progress = (int) ((double) totalRecv * 100.0 / (double) mfile->fileSize);
                if (p != progress) {
                    qDebug("progress:%d", progress);
                    p = progress;
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileProgress(mfile->getUuid(), progress);
                }
                mbyte send;
                if (mfile->isNextStep()) {
                    send = FS_NEXT;
                } else {
                    send = FS_BREAK;
                }
                mioUtil->send(&send, 1);
            } else if (rCmd == FS_END) {
                // 传输完毕
                break;
            } else/* if (rCmd == FS_CLOSE)*/ {
                // 关闭传输
                /*totalRecv = 0;
                break;*/
                return -3;
            }
        } catch (LException &e) {
            thatTotal = 0;
            //           qDebug();
        }
    }
    fileIO.close();
    return thatTotal;
}

QString avoidDuplication(const QFileInfo &outFile) {
    QString name = outFile.fileName();
    if (outFile.exists()) {
        for (int s = 1; s < 65535; s++) {
            QString str;
            if (name.contains(".")) {
                QString prefix = name.mid(0, name.lastIndexOf(".")) + "(" + QString::number(s) + ")";
                QString suffix = name.mid(name.lastIndexOf("."));
                str = prefix + suffix;
            } else {
                str = name + "(" + QString::number(s) + ")";
            }
            QFileInfo fileInfo = QFileInfo(outFile.path(), str);
            if (!fileInfo.exists()) {
                return fileInfo.filePath();
            }
        }
    }
    return outFile.filePath();
}

mlong findFile(std::list<LFile> &listFile, mlong size, const QString &path) {
    QDir dir(path);
    qDebug() << "扫描路径:" << path;
    if (!dir.exists())
        return false;
    dir.setFilter(QDir::Dirs | QDir::Files);
    //    dir.setSorting(QDir::DirsFirst);
    QFileInfoList list = dir.entryInfoList();
    int i = 0;
    mlong fileSize = size;
    do {
        const QFileInfo &fileInfo = list.at(i);
        if (fileInfo.fileName() == "." | fileInfo.fileName() == "..") {
            i++;
            continue;
        }
        if (fileInfo.isDir()) {
            fileSize += findFile(listFile, 0, fileInfo.filePath());
        } else {
            LFile file;
            file.setFileName(fileInfo.fileName());
            file.setIsDirectory(false);
            file.setFileSize(fileInfo.size());
            file.setPath(fileInfo.filePath());
            listFile.push_back(file);
            fileSize += fileInfo.size();
            qDebug() << "扫描到文件: " + fileInfo.filePath() << "大小:" << fileInfo.size();
        }
        i++;
    } while (i < list.size());
    return fileSize;
}

/**
 * 发送文件
 */
void LANShare::sendFile(const Device &device, std::vector<LFile *> selectFiles, int count) {
    std::unique_ptr<TCPClient> client = makeSocket(device.getDevIp(), device.getDevPort());
    if (client == nullptr) {
        return;
    }
    auto *buffer = new mbyte[BUFF_SIZE];
    std::vector<Device>::iterator p1;
    std::vector<Device> mDevices = LANShare::getInstance()->getMDevices();
    for (p1 = mDevices.begin(); p1 != mDevices.end(); p1++) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                  device.getDevIp())) {
            std::string userName = config.clientName.toStdString();
            DataEnc dataEnc(buffer, BUFF_SIZE);
            makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(FS_SHARE_FILE);
            dataEnc.setCount(count);
            dataEnc.putBool(config.encData);
            qDebug() << "IP:" << device.getDevIp();
            try {
                client->send(dataEnc.getData(), dataEnc.getDataLen());
            } catch (LException &e) {
                return;
            }
            for (int i = 0; i < count; ++i) {
                LFile *file = selectFiles[i];
                file->setIndex(i);
                file->setMioUtil(new MTCPClient(client->getFd()));
                file->setUuid(Utils::getUUID());
                if (file->isDirectory()) {
                    std::list<LFile> filelist;
                    mlong fileSize = findFile(filelist, 0, file->getPath());
                    file->setFileSize(fileSize);
                    file->setFileList(filelist);
                }
                qDebug("文件字节大小: %lld", file->fileSize);
                qDebug() << "文件名:" << file->fileName;
                qDebug("文件大小: %s", Utils::computeSize(file->fileSize).c_str());
                dataEnc.reset();
                dataEnc.putLong(file->fileSize);
                dataEnc.putString(file->fileName);
                dataEnc.putInt(file->getFileType());
                dataEnc.putString(std::string(""));
                if (file->isDirectory()) {
                    dataEnc.putInt((int) file->getFileList().size());
                }
                emit
                LANShare::getInstance()->mainWindow->sigRecviceFile(
                    file,
                    file->getUuid(),
                    file->fileName,
                    device.getDevName() + " <- " + config.clientName,
                    file->fileSize,
                    false,
                    !file->isDirectory(),
                    false
                );
                try {
                    client->send(dataEnc.getData(), dataEnc.getDataLen());
                } catch (LException &e) {
                    return;
                }
            }
            try {
                if (!client->recvo(buffer, DataEnc::headerSize())) {
                    qDebug("error");
                    return;
                }
            } catch (LException &e) {
                return;
            }
            DataDec dataDec(buffer, DataEnc::headerSize());
            if (dataDec.getCmd() == FS_NOT_AGREE) {
                qDebug() << device.getDevName() << " 取消接收文件";
                return;
            }
            // 已选择文件列表
            for (int i = 0; i < count; ++i) {
                LFile *file = selectFiles[i];
                mlong total = 0;
                mlong thatTotal = 0;
                // 文件夹发送
                if (file->isDirectory()) {
                    std::list<LFile> fileLis = file->getFileList();
                    QFileInfo fileInfo(file->getPath());
                    QString p = fileInfo.path();
                    p = p.remove(p.size(), 1);
                    for (auto f: fileLis) {
                        QString path = f.getPath();
                        path = path.remove(p);
                        qDebug() << "发送文件:" << path;
                        dataEnc.reset();
                        dataEnc.putLong(f.getFileSize());
                        dataEnc.putString(path);
                        file->mioUtil->send(dataEnc.getData(), dataEnc.getDataLen());
                        f.setUuid(file->getUuid());
                        if (config.encData) {
                            thatTotal = baseSendEnc(file, &f, total, file->getFileSize(), dataEnc);
                        } else {
                            thatTotal = baseSend(file, &f, total, file->getFileSize(), dataEnc);
                        }
                        if (thatTotal == -3) {
                            total = 0;
                            break;
                        } else if (thatTotal <= 0) {
                            continue;
                        }
                        total += thatTotal;
                    }
                } else {
                    // 文件发送
                    if (config.encData) {
                        total += baseSendEnc(file, file, total, file->getFileSize(), dataEnc);
                    } else {
                        total += baseSend(file, file, total, file->getFileSize(), dataEnc);
                    }
                }
                qDebug() << "文件大小:" << file->fileSize << "发送文件大小:" << total;
                if (total != file->fileSize) {
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(file->getUuid(), false, file->getPath());
                } else {
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(file->getUuid(), true, file->getPath());
                    LANShare::getInstance()->lhttpServer->sendWebSocketMessage(
                        file->getFileName(),
                        device.getDevName(),
                        file->getPath(),
                        (!file->isDirectory() && Utils::isPhoto(file->getFileName())) ? 1 : 2,
                        device.getDevMode(),
                        Utils::computeSize(file->getFileSize()).c_str(),
                        false,
                        false,
                        !file->isDirectory()
                    );
                }
                mbyte read = 0;
                // 等待接收方响应再继续发送文件
                client->recvo(&read, 1);
                delete file->mioUtil;
                delete file;
            }
            qDebug("结束");
            client->close();
            break;
        }
    }
    delete[] buffer;
}

void LANShare::startHandleRecvFile(
    bool accept,
    const Device &device,
    bool needEncData,
    const std::vector<LFile *> &files,
    const std::unique_ptr<TCPClient> &tcpClient
) {
    auto *buffer = new mbyte[BUFF_SIZE];
    // 同意接收文件
    DataEnc dataEnc(buffer, BUFF_SIZE);
    if (accept) {
        dataEnc.setCmd(FS_AGREE);
        tcpClient->send(dataEnc.getData(), dataEnc.getDataLen());
        TimeTools::sleep_ms(100);
        for (LFile *item: files) {
            mlong total = 0;
            DataDec dataDec(buffer, BUFF_SIZE);
            if (item->isDirectory()) {
                qDebug() << "接收文件夹" << device.getDevName() << "文件数量:" << item->getSubFileCount() << " 大小:"
                        << item->fileSize;
                for (int i = 0; i < item->getSubFileCount(); ++i) {
                    dataDec.reset();
                    if (tcpClient->recvo(buffer, DataEnc::headerSize()) != DataEnc::headerSize()) return;
                    const int length = dataDec.getLength();
                    if (tcpClient->recvo(buffer, DataEnc::headerSize(), length, 0) != length) return;
                    mlong fileLength = dataDec.getLong();
                    char *c = dataDec.getStr();
                    QString fileName = c;
                    delete[] c;
                    QString saveFilePath = QDir::cleanPath(config.saveFilePath);
                    QString newPath = saveFilePath + SEPARATORS + fileName;
                    const int index = static_cast<int>(newPath.lastIndexOf("/"));
                    QString folder = newPath.mid(0, index);
                    mUtils::createMultipleFolders(folder);
                    qDebug() << "  接收" << device.getDevName() << "文件:" << fileName << " 大小:" << fileLength;
                    QFileInfo outFile(newPath);
                    newPath = avoidDuplication(outFile);
                    IOUtils fileIO(newPath, QFile::WriteOnly);
                    mlong rel;
                    if (needEncData) {
                        rel = baseRecvDec(item, fileIO, dataDec, total);
                    } else {
                        rel = baseRecv(item, fileIO, dataDec, total);
                    }
                    if (rel == -3) {
                        break;
                    } else if (rel <= 0) {
                        continue;
                    }
                    total += rel;
                }
                if (total != item->fileSize) {
                    // 需要删除文件操作
                    qDebug("recv fail");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(
                        item->getUuid(), false,
                        config.saveFilePath + item->getFileName()
                    );
                } else {
                    qDebug("recv sucess");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(
                        item->getUuid(), true,
                        config.saveFilePath + item->getFileName());
                    LHttpServer::sendWebSocketMessage(
                        item->getFileName(),
                        device.getDevName(),
                        "",
                        2,
                        device.getDevMode(),
                        Utils::computeSize(item->getFileSize()).c_str(),
                        true,
                        false,
                        false
                    );
                }
            } else {
                QString saveFilePath = QDir::cleanPath(config.saveFilePath);
                mUtils::createMultipleFolders(saveFilePath);
                qDebug() << "saveFilePath:" << saveFilePath;
                QString newPath = saveFilePath + SEPARATORS + item->fileName;
                QFileInfo outFile(newPath);
                newPath = avoidDuplication(outFile);
                IOUtils fileIO(newPath, QFile::WriteOnly);
                if (needEncData) {
                    total += baseRecvDec(item, fileIO, dataDec, 0);
                } else {
                    total += baseRecv(item, fileIO, dataDec, 0);
                }
                TimeTools::sleep_ms(200);
                if (total != item->fileSize) {
                    // 需要删除文件操作
                    fileIO.deleteFile();
                    qDebug("recv fail");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(item->getUuid(), false,
                                                                         fileIO.getFilePath());
                } else {
                    qDebug("recv sucess");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(item->getUuid(), true,
                                                                         fileIO.getFilePath());
                    const mlong mediaId = item->getMediaId();
                    if (mediaId > -1) {
                        MediaIdPathDBUtil mediaIdPathDbUtil;
                        mediaIdPathDbUtil.addMediaIdPath(mediaId, item->getFileName(), item->getPath(), QDateTime{},
                                                         true);
                    }
                    LHttpServer::sendWebSocketMessage(
                        item->getFileName(),
                        device.getDevName(),
                        newPath,
                        (!item->isDirectory() && Utils::isPhoto(item->getFileName())) ? 1 : 2,
                        device.getDevMode(),
                        Utils::computeSize(item->getFileSize()).c_str(),
                        true,
                        false
                    );
                }
            }
            mbyte send = 2;
            // 响应给发送方继续发送文件
            tcpClient->send(&send, 1);
            delete item->mioUtil;
            delete item;
        }
    } else {
        dataEnc.setCmd(FS_NOT_AGREE);
        tcpClient->send(dataEnc.getData(), dataEnc.getDataLen());
        for (const LFile *item: files) {
            qDebug("Not agree");
            emit
            LANShareWindow::getInstance()->sigRecviceFileSuccess(item->getUuid(), false, "");
        }
    }
    delete[] buffer;
    qDebug("client close");
    tcpClient->close();
}

/**
 * 接收文件操作
 */
void LANShare::handleTcp(std::unique_ptr<TCPClient> tcpClient) {
    auto *buffer = new mbyte[BUFF_SIZE];
    if (tcpClient->recvo(buffer, 4) != 4) return;
    const int magicNum = ByteUtils::bytesToInt(buffer, 0);
    //    qDebug() << "magicNum: " << magicNum;
    if (magicNum != MAGIC_NUM) {
        char buff[5];
        memcpy(buff, buffer, 4);
        buff[4] = '\0';
        QString str(buff);
        str = str.toUpper();
        //        qDebug() << "str:" << buff;
        if (config.webService && (str.startsWith("GET") || str.startsWith("POST"))) {
            getInstance()->lhttpServer->httpServer->newClient(tcpClient.get(), str);
        } else {
            tcpClient->close();
        }
        delete[] buffer;
        return;
    }
    if (tcpClient->recvo(buffer, DataEnc::headerSize()) != DataEnc::headerSize()) return;
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
    int webDeviceCount = dataDec.getInt();
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
    getInstance()->addDevice(device);
    if (webDeviceCount > 0) {
        for (int i = 0; i < webDeviceCount; i++) {
            int webDevicePort = dataDec.getInt();
            QString webDeviceIp = dataDec.getString().c_str();
            QString webDeviceName = dataDec.getString().c_str();
            QString address = webDeviceIp + ":" + QString::number(webDevicePort);
            Device debDevice = getInstance()->onLineDevices[address.toStdString()];
            debDevice.setDevPort(webDevicePort);
            debDevice.setDevIp(webDeviceIp);
            debDevice.setDevName(webDeviceName);
            debDevice.setUniqueUUid(webDeviceIp + QString::number(webDevicePort) + webDeviceName);
            debDevice.setDevMode(Device::L_WEB);
            debDevice.setSetTime(TimeTools::getCurrentTime());
            getInstance()->addDevice(debDevice);
        }
    }
    const int cmd = dataDec.getCmd();
    if (cmd == FS_SHARE_FILE) {
        const int count = dataDec.getCount();
        const boolean needEncData = dataDec.getBool();
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
        if (config.acceptRecvFiles) {
            startHandleRecvFile(true, device, needEncData, files, tcpClient);
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
        const QString decode = mUtils::decMessage(message, config.messageKey);
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
    delete[] buffer;
    delete[] devIp;
    delete[] devName;
    delete[] uniqueUUid;
}

/**
 * 扫描设备
 */
void LANShare::scannDevice() {
    mbyte buffer[2048];
    LANShare *lanShare = LANShare::getInstance();
    while (lanShare->isRun) {
        lanShare->updateMDevices();
        std::vector<Device> devices = lanShare->getMDevices();
        std::vector<Device>::iterator p1;
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            DataEnc dataEnc(buffer, 2048);
            makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_GET_DEVICES);
            UDPClient udpClient(p1->getDevIp());
            //            qDebug() << "广播ip: " << p1->getDevBrotIp();
            //            udpClient.sendto(p1->getDevBrotIp(), config.udpPort, dataEnc.getData(), dataEnc.getDataLen());
            udpSend(&udpClient, &dataEnc, p1->getDevBrotIp(), config.udpPort);
            udpClient.close();
            std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
            lock.lock();
            std::map<std::string, Device>::iterator iter;
            for (iter = lanShare->onLineDevices.begin(); iter != lanShare->onLineDevices.end();) {
                mlong devTime = iter->second.getSetTime();
                mlong currentTime = TimeTools::getCurrentTime();
                mlong timeOut = currentTime - devTime;
                if (iter->second.getCanRemove() && timeOut > (1000 * 20)) {
                    //LLog::Debug("timeOut:%d ip:%s", timeOut, iter->second.getDevIp().c_str());
                    lanShare->onLineDevices.erase(iter++);
                } else {
                    ++iter;
                }
            }
            lock.unlock();
        }
        sleep(5);
        // 打印当前设备数
        //        qDebug("device count:%llu", lanShare->onLineDevices.size());
    }
}

void LANShare::noticeDeviceOnLineByIp(const QString &ip) const {
    std::vector<Device> devices = getMDevices();
    std::vector<Device>::iterator p1;
    for (p1 = devices.begin(); p1 != devices.end(); p1++) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(), ip)) {
            auto *bytes = new mbyte[2048];
            DataEnc dataEnc(bytes, 2048);
            makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_SET_DEVICES);
            UDPClient udpClient(p1->getDevIp());
            udpSend(&udpClient, &dataEnc, ip, config.udpPort);
            delete[] bytes;
            break;
        }
    }
}

void LANShare::noticeDeviceOffLineByIp(const QString &ip) const {
    std::vector<Device> devices = getMDevices();
    std::vector<Device>::iterator p1;
    for (p1 = devices.begin(); p1 != devices.end(); p1++) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(), ip)) {
            auto *bytes = new mbyte[2048];
            DataEnc dataEnc(bytes, 2048);
            makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_DEVICE_OFF_LINE);
            UDPClient udpClient(p1->getDevIp());
            udpSend(&udpClient, &dataEnc, ip, config.udpPort);
            delete[] bytes;
            break;
        }
    }
}

LANShare *LANShare::getInstance() {
    return instance;
}

/**
 * 监听指令
 */
void LANShare::handleUdp() {
    auto *buffer = new mbyte[4096];
    sockaddr_in clientAddr{};
    LANShare *lanShare = LANShare::getInstance();
flag:
    while (lanShare->isRun) {
        int len = lanShare->udpServer->recv(&clientAddr, buffer, 4096);
        if (len <= 0) {
            qDebug("runRecive recv len is <= 0");
            break;
        }
        int magicNum = ByteUtils::bytesToInt(buffer, 0);
        if (magicNum != MAGIC_NUM) {
            continue;
        }
        DataDec dataDec(buffer + 4, len - 4);
        dataDec.decAllData();
        // 设备端口
        int devPort = dataDec.getInt();
        // 设备ip
        char *devIp = dataDec.getStr();
        // 设备名
        char *devName = dataDec.getStr();
        // 设备类型
        int devMode = dataDec.getInt();
        // 设备唯一码
        char *uniqueUUid = dataDec.getStr();
        int dataVersion = dataDec.getInt();
        int batteryLevel = dataDec.getInt();
        mbyte chargeStatus = dataDec.getByte();
        int webDeviceCount = dataDec.getInt();
        //        qDebug() << "devIp：" << devIp;
        // 判断是否为自己发送的指令
        std::vector<Device> devices = lanShare->getMDevices();
        std::vector<Device>::iterator p1;
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            if (strcmp(devIp, p1->getDevIp().toUtf8().data()) == 0) goto flag;
        }
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
        getInstance()->addDevice(device);
        if (webDeviceCount > 0) {
            for (int i = 0; i < webDeviceCount; i++) {
                int webDevicePort = dataDec.getInt();
                QString webDeviceIp = dataDec.getString().c_str();
                QString webDeviceName = dataDec.getString().c_str();
                QString address = webDeviceIp + ":" + QString::number(webDevicePort);
                Device debDevice = getInstance()->onLineDevices[address.toStdString()];
                debDevice.setDevPort(webDevicePort);
                debDevice.setDevIp(webDeviceIp);
                debDevice.setDevName(webDeviceName);
                debDevice.setUniqueUUid(webDeviceIp + QString::number(webDevicePort) + webDeviceName);
                debDevice.setDevMode(Device::L_WEB);
                debDevice.setSetTime(TimeTools::getCurrentTime());
                getInstance()->addDevice(debDevice);
            }
        }
        int cmd = dataDec.getCmd();
        //       qDebug("cmd: %d", cmd);
        if (cmd == UDP_SET_DEVICES) {
            // 添加或覆盖设备
            // qDebug("device on line ip:%s:%d devName:%s devModel:%d", ip, port, devName, devModel);
        } else if (cmd == UDP_GET_DEVICES) {
            //获取设备命令
            lanShare->noticeDeviceOnLineByIp(devIp);
        } else if (cmd == UDP_DEVICE_OFF_LINE) {
            //设备下线
            getInstance()->removeDevice(device);
        } else if (cmd == UDP_MESSAGE) {
            // 消息
            char *message = dataDec.getStr();
            QString decode = mUtils::decMessage(message, config.messageKey);
            qDebug() << "devName:" << devName << " message:" << decode;
            emit
            LANShareWindow::getInstance()->sigNewMessage(device, decode, true);
            lanShare->lhttpServer->sendWebSocketMessage(decode, devName, "", 0, devMode, "", true, false);
            delete[] message;
        } else if (cmd == UDP_MESSAGE_TO_CLIPBOARD) {
            // 消息写入剪切板
            char *message = dataDec.getStr();
            QString decode = mUtils::decMessage(message, config.messageKey);
            qDebug() << "devName:" << devName << " clip message:" << decode;
            emit
            LANShareWindow::getInstance()->sigNewMessage(device, decode, true);
            emit
            LANShareWindow::getInstance()->sigCopyText(decode);
            delete[] message;
        } else if (cmd == UDP_SEND_MEDIA_MUTE) {
            // 暂停
            if (!config.receivceMute) {
                continue;
            }
            qDebug("静音");
#if defined(PLATFORM_WINDOWS)
            try {
                if (!lanShare->muted) {
                    lanShare->systemVolume = Utils::volume();
                    qDebug() << "volume:" << lanShare->systemVolume;
                    Utils::setVolum(0);
                    TimeTools::sleep_ms(500);
                    //                keybd_event(VK_VOLUME_MUTE, 0, 0, 0);
                    //                keybd_event(VK_VOLUME_MUTE, 0, 2, 0);
                    lanShare->muted = true;
                }
            } catch (const std::exception &e) {
            }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
            lanShare->systemVolume = Utils::volume();
            qDebug() << "volume:" << lanShare->systemVolume;
            Utils::setVolum(0);
            lanShare->muted = true;
#endif
        } else if (cmd == UDP_SEND_MEDIA_RESTORE) {
            if (!config.receivceMute) {
                continue;
            }
            qDebug("恢复音量");
#if defined(PLATFORM_WINDOWS)
            try {
                Utils::setVolum(lanShare->systemVolume);
                lanShare->muted = false;
            } catch (const std::exception &e) {
            }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#endif
        }
        delete[] devIp;
        delete[] devName;
        delete[] uniqueUUid;
    }
}

/**
 * 创建TCP服务
 */
void LANShare::createTcpServer() {
    LANShare *lanShare = LANShare::getInstance();
    while (lanShare->isRun) {
        std::unique_ptr<TCPClient> tcpClient = lanShare->tcpServer->accept();
        if (tcpClient == nullptr) {
            return;
        }
        std::thread(handleTcp, std::move(tcpClient)).detach();
    }
}

std::vector<Device> LANShare::getMDevices() const {
    std::mutex &lock = StringLockManager::getStringLock("mDevicesMutex");
    lock.lock();
    std::vector<Device> devices = mDevices;
    lock.unlock();
    return devices;
}

/**
 * 发送消息
 */
void LANShare::broadcastMessage(Device *toDevice, const QString &message, bool isClip, bool shareWS) {
    int strlen = CodeUtils::getUtf8StrLen(message.toUtf8().data());
    if (strlen <= 0) {
        return;
    }
    if (shareWS) {
        lhttpServer->sendWebSocketMessage(message, config.clientName, "", 0, 1, "", false, false);
    }
    QByteArray encMessage = mUtils::encMessage(message, config.messageKey);
    int buffLen = 1024 + encMessage.size();
    auto *buffer = new mbyte[buffLen];
    if (strlen > 700) {
        std::vector<Device> devices = getMDevices();
        std::vector<Device>::iterator p1;
        if (toDevice == nullptr) {
            std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
            lock.lock();
            std::map<std::string, Device>::iterator iter;
            for (iter = onLineDevices.begin(); iter != onLineDevices.end(); iter++) {
                for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                    if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                              iter->second.getDevIp())) {
                        DataEnc dataEnc(buffer, buffLen);
                        makeDataEnc(*p1, &dataEnc);
                        dataEnc.setCmd(FS_MESSAGE);
                        dataEnc.putString(encMessage);
                        std::unique_ptr<TCPClient> socket = makeSocket(iter->second.getDevIp(),
                                                                       iter->second.getDevPort());
                        if (socket == nullptr) {
                            break;
                        }
                        socket->send(dataEnc.getData(), dataEnc.getDataLen());
                        break;
                    }
                }
            }
            lock.unlock();
        } else {
            for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                          toDevice->getDevIp())) {
                    DataEnc dataEnc(buffer, buffLen);
                    dataEnc.setCmd(FS_MESSAGE);
                    makeDataEnc(*p1, &dataEnc);
                    dataEnc.putString(encMessage);
                    std::unique_ptr<TCPClient> socket = makeSocket(toDevice->getDevIp(), toDevice->getDevPort());
                    if (socket == nullptr) {
                        break;
                    }
                    socket->send(dataEnc.getData(), dataEnc.getDataLen());
                    break;
                }
            }
        }
        return;
    }
    std::vector<Device> devices = getMDevices();
    std::vector<Device>::iterator p1;
    if (toDevice == nullptr) {
        std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
        lock.lock();
        std::map<std::string, Device>::iterator iter;
        for (iter = onLineDevices.begin(); iter != onLineDevices.end(); iter++) {
            for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                          iter->second.getDevIp())) {
                    DataEnc dataEnc(buffer, buffLen);
                    makeDataEnc(*p1, &dataEnc);
                    dataEnc.setCmd(isClip ? UDP_MESSAGE_TO_CLIPBOARD : UDP_MESSAGE);
                    dataEnc.putString(encMessage);
                    udpSend(udpServer.get(), &dataEnc, iter->second.getDevIp(), config.udpPort);
                    break;
                }
            }
        }
        lock.unlock();
    } else {
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                      toDevice->getDevIp())) {
                DataEnc dataEnc(buffer, buffLen);
                dataEnc.setCmd(isClip ? UDP_MESSAGE_TO_CLIPBOARD : UDP_MESSAGE);
                makeDataEnc(*p1, &dataEnc);
                dataEnc.putString(encMessage);
                udpSend(udpServer.get(), &dataEnc, toDevice->getDevIp(), config.udpPort);
                break;
            }
        }
    }
    delete[] buffer;
}

const std::map<std::string, Device> &LANShare::getOnLineDevices() const {
    return onLineDevices;
}

void LANShare::close() {
    // 通知设备我已下线
    for (const auto &device: getMDevices()) {
        this->noticeDeviceOffLineByIp(device.getDevBrotIp());
    }
    udpServer->close();
    tcpServer->close();
    isRun = false;
    qDebug() << "LANShare Server is closed";
}
