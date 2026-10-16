//
// Created by fgsq on 2026/10/12.
//

#include "FileSendV5.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <thread>
#include <vector>

#include "ByteArrayIOUtils.h"
#include "Config.hpp"
#include "CustomDataStream.h"
#include "FileServer.h"
#include "IOUtils.h"
#include "LANShareWindow.h"
#include "LHttpServer.h"
#include "ResumeStore.h"
#include "TCPClient.h"

// V5 发送进度回调：与 V4 行为一致，仅负责驱动 UI 进度
class FileSendV5ProgressCallback : public ProgressCallback {
public:
    void onStart(FileTransfer *fileTransfer) override {
        auto devName = fileTransfer->fromDevice.getDevName();
        for (auto file: fileTransfer->files) {
            file->setUuid(Utils::getUUID());
            emit
            LANShareWindow::getInstance()->sigReceiveFile(
                file,
                file->getUuid(),
                file->getFileName(),
                devName,
                file->getFileSize(),
                false,
                !file->isDirectory(),
                false
            );
        }
    }

    void onProgress(FileTransfer *fileTransfer, LFile *fileItem) override {
        LANShareWindow::getInstance()->sigReceiveFileProgress(fileItem->getUuid(), fileItem->getProgress());
    }

    void onFinish(FileTransfer *fileTransfer, LFile *fileItem, bool isSuccess) override {
        if (!isSuccess) {
            qDebug("send fail");
            emit
            LANShareWindow::getInstance()->sigReceiveFileSuccess(
                fileItem->getUuid(), false,
                fileItem->getPath()
            );
        } else {
            qDebug("send sucess");
            emit
            LANShareWindow::getInstance()->sigReceiveFileSuccess(
                fileItem->getUuid(), true,
                fileItem->getPath());
            LHttpServer::sendWebSocketMessage(
                fileItem->getFileName(),
                fileTransfer->toDevice.getDevName(),
                "",
                2,
                fileTransfer->toDevice.getDevMode(),
                Utils::computeSize(fileItem->getFileSize()).c_str(),
                false,
                false,
                false
            );
        }
    }
};

FileSendV5::FileSendV5() {
    callback = new FileSendV5ProgressCallback();
}

void FileSendV5::send(Device fromDevice, Device toDevice, std::unique_ptr<TCPClient> tcpClient,
                      std::vector<LFile *> fileList) {
    CustomDataStream dataStream(tcpClient.get());
    bool resumeEnabled = toDevice.getDataVersion() >= DATA_VERSION_5;
    QString senderUuid = fromDevice.getUniqueUUid();
    sendNewVersionFlag(fromDevice, {tcpClient.get()});
    QJsonArray fileListJson;
    for (auto file: fileList) {
        QJsonObject fileJson;
        if (file->getFileType() == FILE_FOLDER) {
            std::vector<LFile> children;
            mlong fileSize = LFile::findFileNew(children, 0, file->getPath(), file->getPath());
            file->setFileSize(fileSize);
            QJsonArray childFileJsonArray;
            for (auto &child: children) {
                if (resumeEnabled && child.getFileSize() > 0 && !child.getPath().isEmpty()) {
                    child.setFileId(ResumeStore::stableId(
                        senderUuid + "|" + child.getPath() + "|" + QString::number(child.getFileSize())));
                } else {
                    child.setFileId(Utils::getUUID());
                }
                QJsonObject childFileJson;
                childFileJson["fileId"] = child.getFileId();
                childFileJson["name"] = child.getFileName();
                childFileJson["length"] = static_cast<qlonglong>(child.getFileSize());
                childFileJson["fileType"] = FILE_TYPE_FILE;
                childFileJsonArray.push_back(childFileJson);
            }
            file->setFileList(children);
            fileJson["fileType"] = FILE_TYPE_FOLDER;
            fileJson["children"] = childFileJsonArray;
        } else {
            fileJson["fileType"] = FILE_TYPE_FILE;
        }
        // 顶层文件 fileId：续传用稳定 id，否则随机
        if (resumeEnabled && !file->getPath().isEmpty()) {
            QString seed = (file->getFileType() == FILE_FOLDER)
                           ? (senderUuid + "|folder|" + file->getPath())
                           : (senderUuid + "|" + file->getPath() + "|" + QString::number(file->getFileSize()));
            file->setFileId(ResumeStore::stableId(seed));
        } else {
            file->setFileId(Utils::getUUID());
        }
        fileJson["fileId"] = file->getFileId();
        fileJson["toUser"] = toDevice.getDevName();
        fileJson["name"] = file->getFileName();
        fileJson["length"] = static_cast<qlonglong>(file->getFileSize());
        fileListJson.push_back(fileJson);
    }
    QJsonObject fileTransferJson;
    fileTransferJson["fromDevice"] = fromDevice.toJsonObject();
    fileTransferJson["toDevice"] = toDevice.toJsonObject();
    fileTransferJson["files"] = fileListJson;
    if (resumeEnabled) {
        fileTransferJson["resume"] = 1;
    }
    try {
        dataStream.writeInt(FS_SHARE_FILE);
        dataStream.writeBoolean(Config::instance().encData);
        dataStream.writeString(QJsonDocument(fileTransferJson).toJson().data());
        int response = dataStream.readInt();
        if (response != FS_AGREE) {
            qDebug("对方拒绝接收文件");
            return;
        }
        FileTransferV5 fileTransfer;
        fileTransfer.fromDevice = fromDevice;
        fileTransfer.toDevice = toDevice;
        fileTransfer.type = FILE_TYPE_FILE;
        fileTransfer.files = fileList;
        fileTransfer.resumeEnabled = resumeEnabled;
        if (resumeEnabled) {
            fileTransfer.setResumeMap(readResumeMap(dataStream));
        }
        callback->onStart(&fileTransfer);
        handleFileTransfer(&fileTransfer, tcpClient.get());
    } catch (const std::exception &e) {
        qDebug() << "send error: " << e.what();
    }
}

void FileSendV5::handleFileTransfer(FileTransferV5 *fileTransfer, TCPClient *tcpClient) const {
    auto *dataStream = new CustomDataStream(tcpClient);
    std::thread t([fileTransfer, dataStream]() {
        try {
            while (true) {
                auto fileId = dataStream->readString();
                if (fileId.empty()) {
                    qDebug("file id is empty");
                    return;
                }
                for (auto file: fileTransfer->files) {
                    if (file->getFileId() == QString(fileId.c_str())) {
                        file->cancelFileTransfer();
                        break;
                    }
                }
            }
        } catch (std::exception &e) {
            qDebug("read fileId error");
        }
    });
    for (auto file: fileTransfer->files) {
        if (file->getFileType() == FILE_FOLDER) {
            sendFolder(fileTransfer, dataStream, file);
        } else {
            sendFile(fileTransfer, dataStream, file);
        }
    }
    t.join();
    delete dataStream;
}

/**
 * 发送文件夹（支持逐子文件续传）
 */
void FileSendV5::sendFolder(FileTransferV5 *fileTransfer, CustomDataStream *dataStream, LFile *fileItem) const {
    mlong fileSize = fileItem->getFileSize();
    std::vector<LFile> children = fileItem->getFileList();
    mlong subTotal = 0;
    for (auto &child: children) {
        if (child.getFileSize() <= 0) {
            continue;
        }
        mlong childOffset = clampOffset(fileTransfer->getResumeOffset(child.getFileId()), child.getFileSize());
        if (childOffset >= child.getFileSize()) {
            // 该子文件接收端已完整持有，跳过传输
            subTotal += child.getFileSize();
            continue;
        }
        mlong ten;
        if (Config::instance().encData) {
            ten = sendFileStreamEnc(fileTransfer, subTotal + childOffset, fileSize, fileItem, &child, dataStream,
                                    childOffset);
        } else {
            ten = sendFileStream(fileTransfer, subTotal + childOffset, fileSize, fileItem, &child, dataStream,
                                 childOffset);
        }
        if (ten < 0) {
            break;
        }
        subTotal += ten;
    }
    callback->onFinish(fileTransfer, fileItem, subTotal == fileSize);
}

/**
 * 发送单个文件（支持续传）
 */
void FileSendV5::sendFile(FileTransferV5 *fileTransfer, CustomDataStream *dataStream, LFile *fileItem) const {
    QString filePath = fileItem->getPath();
    mlong length = fileItem->getFileSize();
    mlong total = 0;
    if (length > 0) {
        mlong startOffset = clampOffset(fileTransfer->getResumeOffset(fileItem->getFileId()), length);
        if (startOffset >= length) {
            total = length;
        } else if (Config::instance().encData) {
            total = sendFileStreamEnc(fileTransfer, startOffset, length, fileItem, fileItem, dataStream, startOffset);
        } else {
            total = sendFileStream(fileTransfer, startOffset, length, fileItem, fileItem, dataStream, startOffset);
        }
    }
    callback->onFinish(fileTransfer, fileItem, length == total);
}

/**
 * 发送文件流（未加密，带续传起点）
 *
 * @param progressBase 进度基准（文件夹为已累加字节数，单文件为续传偏移）
 * @param folderSize   进度分母（单文件为自身长度，文件夹为文件夹总长）
 * @param startOffset  该文件内已完成的绝对字节偏移
 * @return 成功返回该文件的绝对完成字节数（==length），失败返回 -1
 */
mlong FileSendV5::sendFileStream(
    FileTransferV5 *fileTransfer, mlong progressBase, mlong folderSize, LFile *baseFileItem, LFile *fileItem,
    CustomDataStream *dataStream, mlong startOffset
) const {
    IOInter *ioUtils;
    if (fileItem->getType() == LFile::BYTEARRAY) {
        ioUtils = new ByteArrayIOUtils(fileItem->getByteArray());
    } else if (fileItem->getType() == LFile::FILE) {
        ioUtils = new IOUtils(fileItem->path, QFile::ReadOnly);
    } else if (fileItem->getType() == LFile::STREAM) {
        ioUtils = fileItem->getIoInter();
    } else {
        qDebug("发送的文件类型不支持");
        return -1;
    }
    int len;
    mlong fileIndex = startOffset;
    mlong sent = 0;
    mlong targetSize = fileItem->getFileSize() - startOffset;
    int progress;
    int lastProgress = 0;
    std::vector<mbyte> buffer(1024 * 1024); // 1MB 缓冲区
    try {
        ioUtils->setSeek(startOffset);
        while (true) {
            if (!baseFileItem->isNextStep()) {
                dataStream->writeInt(NEW_FS_BREAK);
                return -1;
            }
            len = ioUtils->read(buffer.data(), 1024 * 1024);
            if (len <= 0) {
                break;
            }
            dataStream->writeInt(len);
            dataStream->write(buffer.data(), len);
            fileIndex += len;
            sent += len;
            targetSize -= len;
            // 全 long 运算，避免大文件 (progressBase+sent)*100 溢出 int 导致进度卡在 0
            progress = folderSize > 0 ? static_cast<int>((progressBase + sent) * 100 / folderSize) : 100;
            if (progress > lastProgress) {
                lastProgress = progress;
                baseFileItem->setProgress(progress);
                callback->onProgress(fileTransfer, baseFileItem);
            }
            if (targetSize <= 0) {
                break;
            }
        }
    } catch (std::exception &e) {
        qDebug("send file error");
        return -1;
    }
    ioUtils->close();
    if (fileIndex != fileItem->getFileSize()) {
        return -1;
    }
    return fileIndex;
}

/**
 * 发送文件流（加密，带续传起点）
 */
mlong FileSendV5::sendFileStreamEnc(
    FileTransferV5 *fileTransfer, mlong progressBase, mlong folderSize, LFile *baseFileItem, LFile *fileItem,
    CustomDataStream *dataStream, mlong startOffset
) const {
    auto *input = new IOUtils(fileItem->getPath(), QFile::ReadOnly);
    if (!input->isOpen()) {
        qDebug("file not open");
        return -1;
    }
    int len;
    mlong fileIndex = startOffset;
    mlong sent = 0;
    mlong targetSize = fileItem->getFileSize() - startOffset;
    int progress;
    int lastProgress = 0;
    std::vector<mbyte> buffer(1024 * 1024); // 1MB 缓冲区
    try {
        input->setSeek(startOffset);
        while (true) {
            if (!baseFileItem->isNextStep()) {
                dataStream->writeInt(NEW_FS_BREAK);
                return -1;
            }
            len = input->read(buffer.data(), 1024 * 1024);
            if (len <= 0) {
                break;
            }
            // 加密：index 为文件内绝对字节偏移，与接收端解密对齐
            mUtils::encData(buffer.data(), len, 0, fileIndex);
            dataStream->writeInt(len);
            dataStream->write(buffer.data(), len);
            fileIndex += len;
            sent += len;
            targetSize -= len;
            progress = folderSize > 0 ? static_cast<int>((progressBase + sent) * 100 / folderSize) : 100;
            if (progress > lastProgress) {
                lastProgress = progress;
                baseFileItem->setProgress(progress);
                callback->onProgress(fileTransfer, baseFileItem);
            }
            if (targetSize <= 0) {
                break;
            }
        }
    } catch (std::exception &e) {
        qDebug("send file error");
        return -1;
    }
    input->close();
    if (fileIndex != fileItem->getFileSize()) {
        return -1;
    }
    return fileIndex;
}

void FileSendV5::sendNewVersionFlag(const Device &fromDevice, CustomDataStream dataStream) {
    QJsonObject deviceJson;
    deviceJson["devName"] = fromDevice.getDevName();
    deviceJson["devMode"] = fromDevice.getDevMode();
    deviceJson["devPort"] = fromDevice.getDevPort();
    deviceJson["devIp"] = fromDevice.getDevIp();
    deviceJson["devBrotIp"] = fromDevice.getDevBrotIp();
    deviceJson["devNetMask"] = fromDevice.getDevNetMask();
    deviceJson["uniqueUUid"] = fromDevice.getUniqueUUid();
    deviceJson["dataVersion"] = fromDevice.getDataVersion();
    deviceJson["batteryLevel"] = fromDevice.getBatteryLevel();
    deviceJson["chargeStatus"] = fromDevice.getChargeStatus();
    dataStream.writeInt(NEW_VERSION_5);
    // 发送设备信息
    dataStream.writeString(QJsonDocument(deviceJson).toJson().data());
}

std::map<QString, mlong> FileSendV5::readResumeMap(CustomDataStream &dataStream) {
    std::map<QString, mlong> map;
    std::string json = dataStream.readString();
    if (json.empty()) {
        return map;
    }
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(json.c_str(), static_cast<int>(json.size())));
    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        map[it.key()] = it.value().toVariant().toLongLong();
    }
    return map;
}

mlong FileSendV5::clampOffset(mlong offset, mlong length) {
    if (offset < 0) {
        return 0;
    }
    return offset > length ? length : offset;
}
