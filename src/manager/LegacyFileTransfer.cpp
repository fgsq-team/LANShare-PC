//
// Created by fgsq on 2026/10/6.
//

#include "LegacyFileTransfer.hpp"
#include "LANShare.h"
#include "TcpProtocol.hpp"
#include "LHttpServer.h"
#include "NetWorldUtils.h"
#include "TimeTools.h"
#include "Utils.h"
#include "LException.h"
#include "IOUtils.h"
#include "ByteArrayIOUtils.h"
#include "mUtils.h"
#include "MediaIdPathDBUtil.h"
#include "Config.hpp"


#define BUFF_SIZE (1024 * 1024 * 2)

/**
 * 构造函数
 */
LegacyFileTransfer::LegacyFileTransfer(LANShare *lanshare) : lanshare(lanshare) {}

/**
 * 基础文件发送（不加密）
 * 从文件/字节数组/流中读取数据并发送
 * @return 实际发送的字节数，失败返回负数
 */
mlong LegacyFileTransfer::baseSend(LFile *file, LFile *f, mlong size, mlong totalFileSize, DataEnc &dataEnc) {
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

/**
 * 基础文件发送（加密）
 * 从文件/字节数组/流中读取数据，加密后发送
 * @return 实际发送的字节数，失败返回负数
 */
mlong LegacyFileTransfer::baseSendEnc(LFile *file, LFile *f, mlong size, mlong totalFileSize, DataEnc &dataEnc) {
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
            mUtils::encData(dataEnc.getBuffer(), ten, DataEnc::headerSize(), thatSend);
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

/**
 * 基础文件接收（不加密）
 * 接收数据并写入文件
 * @return 实际接收的字节数，失败返回负数
 */
mlong LegacyFileTransfer::baseRecv(const LFile *mfile, IOUtils &fileIO, DataDec &dataDec, mlong mTotalRecv) {
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
                break;
            } else {
                return -3;
            }
        } catch (LException &e) {
            thatTotal = 0;
        }
    }
    fileIO.close();
    return thatTotal;
}

/**
 * 基础文件接收（解密）
 * 接收加密数据，解密后写入文件
 * @return 实际接收的字节数，失败返回负数
 */
mlong LegacyFileTransfer::baseRecvDec(const LFile *mfile, IOUtils &fileIO, DataDec &dataDec, mlong mTotalRecv) {
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
                int length = dataDec.getLength();
                if (mioUtil->recvo(buffer, DataEnc::headerSize(), length, 0) != length) {
                    qDebug("recvo error");
                    break;
                }
                mUtils::decData(buffer, length, DataEnc::headerSize(), thatTotal);
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
                break;
            } else {
                return -3;
            }
        } catch (LException &e) {
            thatTotal = 0;
        }
    }
    fileIO.close();
    return thatTotal;
}

/**
 * 发送文件到指定设备
 * 建立 TCP 连接，发送文件列表，然后逐个发送文件内容
 */
void LegacyFileTransfer::sendFile(const Device &device, std::vector<LFile *> selectFiles, int count) {
    std::unique_ptr<TCPClient> client = TcpProtocol::makeSocket(device.getDevIp(), device.getDevPort());
    if (client == nullptr) {
        return;
    }

    auto *buffer = new mbyte[BUFF_SIZE];
    std::vector<Device>::iterator p1;
    std::vector<Device> mDevices = lanshare->getMDevices();
    for (p1 = mDevices.begin(); p1 != mDevices.end(); ++p1) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                  device.getDevIp())) {
            if (device.getDataVersion() >= DATA_VERSION_4) {
                lanshare->fileSend.send(*p1, device, std::move(client), selectFiles);
                return;
            }
            DataEnc dataEnc(buffer, BUFF_SIZE);
            TcpProtocol::makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(FS_SHARE_FILE);
            dataEnc.setCount(count);
            dataEnc.putBool(Config::instance().encData);
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
                    std::vector<LFile> filelist;
                    mlong fileSize = LFile::findFile(filelist, 0, file->getPath());
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
                lanshare->mainWindow->sigRecviceFile(
                    file,
                    file->getUuid(),
                    file->fileName,
                    device.getDevName() + " <- " + Config::instance().clientName,
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
            for (int i = 0; i < count; ++i) {
                LFile *file = selectFiles[i];
                mlong total = 0;
                mlong thatTotal = 0;
                if (file->isDirectory()) {
                    std::vector<LFile> fileLis = file->getFileList();
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
                        if (Config::instance().encData) {
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
                    if (Config::instance().encData) {
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
                    lanshare->lhttpServer->sendWebSocketMessage(
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

/**
 * 开始处理接收文件
 * 根据是否同意接收，发送同意/拒绝响应，然后接收文件内容
 */
void LegacyFileTransfer::startHandleRecvFile(
    bool accept,
    const Device &device,
    bool needEncData,
    const std::vector<LFile *> &files,
    const std::unique_ptr<TCPClient> &tcpClient
) {
    auto *buffer = new mbyte[BUFF_SIZE];
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
                    QString saveFilePath = QDir::cleanPath(Config::instance().saveFilePath);
                    QString newPath = saveFilePath + SEPARATORS + fileName;
                    const int index = static_cast<int>(newPath.lastIndexOf("/"));
                    QString folder = newPath.mid(0, index);
                    mUtils::createMultipleFolders(folder);
                    qDebug() << "  接收" << device.getDevName() << "文件:" << fileName << " 大小:" << fileLength;
                    QFileInfo outFile(newPath);
                    newPath = mUtils::avoidDuplication(outFile);
                    IOUtils fileIO(newPath, QFile::WriteOnly);
                    mlong rel;
                    if (needEncData) {
                        rel = baseRecvDec(item, fileIO, dataDec, total);
                    } else {
                        rel = baseRecv(item, fileIO, dataDec, total);
                    }
                    if (rel == -3) {
                        break;
                    }
                    if (rel <= 0) {
                        continue;
                    }
                    total += rel;
                }
                if (total != item->fileSize) {
                    qDebug("recv fail");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(
                        item->getUuid(), false,
                        Config::instance().saveFilePath + item->getFileName()
                    );
                } else {
                    qDebug("recv sucess");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(
                        item->getUuid(), true,
                        Config::instance().saveFilePath + item->getFileName());
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
                QString saveFilePath = QDir::cleanPath(Config::instance().saveFilePath);
                mUtils::createMultipleFolders(saveFilePath);
                qDebug() << "saveFilePath:" << saveFilePath;
                QString newPath = saveFilePath + SEPARATORS + item->fileName;
                QFileInfo outFile(newPath);
                newPath = mUtils::avoidDuplication(outFile);
                IOUtils fileIO(newPath, QFile::WriteOnly);
                if (needEncData) {
                    total += baseRecvDec(item, fileIO, dataDec, 0);
                } else {
                    total += baseRecv(item, fileIO, dataDec, 0);
                }
                TimeTools::sleep_ms(200);
                if (total != item->fileSize) {
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
 * 新版文件接收处理
 * 委托给 FileServer 处理新版协议的文件接收
 */
void LegacyFileTransfer::startNewVersionHandleRecvFile(
    Device fromDevice, FileTransfer *fileTransfer, std::vector<LFile *> files, CustomDataStream *stream,
    bool encData, bool isAgree
) {
    lanshare->fileServer.startReceiveFile(fileTransfer, std::move(files), stream, encData, isAgree);
}
