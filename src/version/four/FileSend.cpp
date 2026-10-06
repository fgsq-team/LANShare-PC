//
// Created by 16508 on 2025/8/30.
//

#include "FileSend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <thread>

#include "CustomDataStream.h"
#include "FileServer.h"
#include "LANShareWindow.h"
#include "LHttpServer.h"
#include "ProgressCallback.h"

// 具体实现
class FileSendProgressCallback : public ProgressCallback {
public:
    void onStart(FileTransfer *fileTransfer) override {
        auto devName = fileTransfer->fromDevice.getDevName();
        for (auto file: fileTransfer->files) {
            file->setUuid(Utils::getUUID());
            emit
            LANShareWindow::getInstance()->sigRecviceFile(
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
        LANShareWindow::getInstance()->sigRecviceFileProgress(fileItem->getUuid(), fileItem->getProgress());
    }

    void onFinish(FileTransfer *fileTransfer, LFile *fileItem, bool isSuccess) override {
        if (!isSuccess) {
            // 需要删除文件操作
            qDebug("send fail");
            emit
            LANShareWindow::getInstance()->sigRecviceFileSuccess(
                fileItem->getUuid(), false,
                fileItem->getPath()
            );
        } else {
            qDebug("recv sucess");
            emit
            LANShareWindow::getInstance()->sigRecviceFileSuccess(
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

FileSend::FileSend() {
    callback = new FileSendProgressCallback();
}

void FileSend::send(Device fromDevice, Device toDevice, std::unique_ptr<TCPClient> tcpClient,
                    std::vector<LFile *> fileList) {
    CustomDataStream dataStream(tcpClient.get());
    sendNewVersionFlag(fromDevice, {tcpClient.get()});
    QString devName = fromDevice.getDevName();
    QJsonArray fileListJson;
    for (auto file: fileList) {
        file->setFileId(Utils::getUUID());
        QJsonObject fileJson;
        fileJson["fileId"] = file->getFileId();
        fileJson["toUser"] = toDevice.getDevName();
        if (file->getFileType() == FILE_FOLDER) {
            std::vector<LFile> filelist;
            mlong fileSize = LFile::findFileNew(filelist, 0, file->getPath(), file->getPath());
            file->setFileSize(fileSize);
            file->setFileList(filelist);
            fileJson["fileType"] = FILE_TYPE_FOLDER;
            QJsonArray childFileJsonArray;
            for (const auto &childFile: filelist) {
                QJsonObject childFileJson;
                childFileJson["name"] = childFile.getFileName();
                childFileJson["length"] = childFile.getFileSize();
                childFileJson["fileType"] = FILE_TYPE_FILE;
                childFileJsonArray.push_back(childFileJson);
            }
            fileJson["children"] = childFileJsonArray;
        } else {
            fileJson["fileType"] = FILE_TYPE_FILE;
        }
        fileJson["name"] = file->getFileName();
        fileJson["length"] = file->getFileSize();
        fileListJson.push_back(fileJson);
    }
    QJsonObject fileTransferJson;
    fileTransferJson["fromDevice"] = fromDevice.toJsonObject();
    fileTransferJson["toDevice"] = toDevice.toJsonObject();
    fileTransferJson["files"] = fileListJson;
    try {
        dataStream.writeInt(FS_SHARE_FILE);
        dataStream.writeBoolean(Config::instance().encData);
        dataStream.writeString(QJsonDocument(fileTransferJson).toJson().data());
        int response = dataStream.readInt();
        if (response != FS_AGREE) {
            qDebug("对方拒绝接收文件");
            return;
        }
        FileTransfer fileTransfer;
        fileTransfer.fromDevice = fromDevice;
        fileTransfer.toDevice = toDevice;
        fileTransfer.type = FILE_TYPE_FILE;
        fileTransfer.files = fileList;
        callback->onStart(&fileTransfer);
        handleFileTransfer(&fileTransfer, tcpClient.get());
    } catch (const std::exception &e) {
        qDebug() << "send error: " << e.what();
    }
}

void FileSend::handleFileTransfer(FileTransfer *fileTransfer, TCPClient *tcpClient) const {
    auto *dataStream = new CustomDataStream(tcpClient);
    std::thread t([fileTransfer,dataStream]() {
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
 * 发送文件夹
 */
void FileSend::sendFolder(FileTransfer *fileTransfer, CustomDataStream *dataStream, LFile *fileItem) const {
    mlong fileSize = fileItem->getFileSize();
    std::vector<LFile> children = fileItem->getFileList();
    mlong subTotal = 0;
    for (LFile child: children) {
        if (child.getFileSize() <= 0) {
            continue;
        }
        mlong ten;
        if (Config::instance().encData) {
            ten = sendFileStreamEnc(fileTransfer, subTotal, fileSize, fileItem, &child, dataStream);
        } else {
            ten = sendFileStream(fileTransfer, subTotal, fileSize, fileItem, &child, dataStream);
        }
        if (ten < 0) {
            break;
        }
        subTotal += ten;
    }
    callback->onFinish(fileTransfer, fileItem, subTotal == fileSize);
}

/**
  * 发送文件
  */
void FileSend::sendFile(FileTransfer *fileTransfer, CustomDataStream *dataStream, LFile *fileItem) const {
    // 服务端同意接收文件，开始发送文件内容
    QString filePath = fileItem->getPath();
    mlong total = 0;
    if (fileItem->getFileSize() > 0) {
        if (Config::instance().encData) {
            total = sendFileStreamEnc(fileTransfer, 0, fileItem->getFileSize(), fileItem, fileItem, dataStream);
        } else {
            total = sendFileStream(fileTransfer, 0, fileItem->getFileSize(), fileItem, fileItem, dataStream);
        }
    }
    callback->onFinish(fileTransfer, fileItem, fileItem->getFileSize() == total);
}

mlong FileSend::sendFileStream(
    FileTransfer *fileTransfer, mlong total, mlong fileSize, LFile *baseFileItem, LFile *fileItem,
    CustomDataStream *dataStream
) const {
    auto *input = new IOUtils(fileItem->getPath(), QFile::ReadOnly);
    if (!input->isOpen()) {
        qDebug("file not open");
        return -1;
    }
    int len;
    mlong subTotal = 0;
    mlong targetSize = fileItem->getFileSize();
    int progress;
    int lastProgress = 0;
    auto *buffer = new mbyte[1024 * 1024]; // 1MB缓冲区
    try {
        while (true) {
            if (!baseFileItem->isNextStep()) {
                dataStream->writeInt(NEW_FS_BREAK);
                break;
            }
            len = input->read(buffer, 1024 * 1024);
            if (len <= 0) {
                break;
            }
            dataStream->writeInt(len);
            dataStream->write(buffer, len);
            subTotal += len;
            total += len;
            targetSize -= len;
            progress = static_cast<int>(total * 100 / fileSize);
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
    if (subTotal != fileItem->getFileSize()) {
        subTotal = -1;
    }
    delete[] buffer;
    return subTotal;
}

mlong FileSend::sendFileStreamEnc(FileTransfer *fileTransfer, mlong total, mlong folderSize, LFile *baseFileItem,
                                  LFile *fileItem, CustomDataStream *dataStream) const {
    auto *input = new IOUtils(fileItem->getPath(), QFile::ReadOnly);
    if (!input->isOpen()) {
        qDebug("file not open");
        return -1;
    }
    int len;
    mlong subTotal = 0;
    mlong targetSize = fileItem->getFileSize();
    int progress;
    int lastProgress = 0;
    auto *buffer = new mbyte[1024 * 1024]; // 1MB缓冲区
    try {
        while (true) {
            if (!baseFileItem->isNextStep()) {
                dataStream->writeInt(NEW_FS_BREAK);
                break;
            }
            len = input->read(buffer, 1024 * 1024);
            if (len <= 0) {
                break;
            }
            mUtils::encData(buffer, len, 0, subTotal);
            dataStream->writeInt(len);
            dataStream->write(buffer, len);
            subTotal += len;
            total += len;
            targetSize -= len;
            progress = static_cast<int>(total * 100 / folderSize);
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
    if (subTotal != fileItem->getFileSize()) {
        subTotal = -1;
    }
    delete[] buffer;
    return subTotal;
}

void FileSend::sendNewVersionFlag(const Device &fromDevice, CustomDataStream dataStream) {
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
    dataStream.writeInt(NEW_VERSION_4);
    // 发送设备信息
    dataStream.writeString(QJsonDocument(deviceJson).toJson().data());
}
