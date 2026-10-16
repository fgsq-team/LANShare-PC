//
// Created by fgsq on 2026/10/12.
//

#include "FileServerV5.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <vector>

#include "Config.hpp"
#include "FileServer.h"
#include "LANShare.h"
#include "LANShareWindow.h"
#include "LHttpServer.h"
#include "ResumeStore.h"
#include "SyncFeatureHandler.h"
#include "TCPClient.h"
#include "mUtils.h"

// V5 接收进度回调：与 V4 行为一致，仅负责驱动 UI 进度
class FileServerV5ProgressCallback : public ProgressCallback {
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
                true,
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
            qDebug("recv fail");
            emit
            LANShareWindow::getInstance()->sigReceiveFileSuccess(
                fileItem->getUuid(), false,
                ""
            );
        } else {
            qDebug("recv sucess");
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
                true,
                false,
                false
            );
        }
    }
};

FileServerV5::FileServerV5() {
    callback = new FileServerV5ProgressCallback();
    // 启动时清理过期分段
    ResumeStore::cleanupExpired();
}

void FileServerV5::handleVersion(const Device &device, std::unique_ptr<TCPClient> tcpClient) {
    auto stream = new CustomDataStream(tcpClient.get());
    int cmd = stream->readInt();
    switch (cmd) {
        case FS_SHARE_FILE: // V5 文件传输（断点续传）
            handleFileTransfer(device, stream, std::move(tcpClient));
            break;
        case FS_MESSAGE: // 接收消息（与文件传输无关，V4/V5 共用 SyncFeatureHandler）
            SyncFeatureHandler::handleMessage(device, stream);
            break;
        case FS_GET_NO_SYNC_MEDIA: // 媒体同步（V4/V5 共用 SyncFeatureHandler）
            SyncFeatureHandler::handleMediaSync(device, stream);
            break;
        default:
            break;
    }
}

void FileServerV5::handleFileTransfer(const Device &fromDevice, CustomDataStream *stream,
                                      std::unique_ptr<TCPClient> tcpClient) {
    bool encData = stream->readBoolean();
    std::string jsonStr = stream->readString();
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonStr.c_str(), static_cast<int>(jsonStr.size())));
    QJsonObject data = doc.object();
    QJsonArray filesJson = data["files"].toArray();
    std::vector<LFile *> files;
    for (const auto &fileJson: filesJson) {
        auto jsonObject = fileJson.toObject();
        int fileType = jsonObject["fileType"].toInt();
        auto *fileItem = new LFile();
        fileItem->setFileName(jsonObject["name"].toString());
        fileItem->setFileSize(jsonObject["length"].toVariant().toLongLong());
        fileItem->setFileId(jsonObject["fileId"].toString());
        if (fileType == FILE_TYPE_FOLDER) {
            fileItem->setFileType(FILE_FOLDER);
            fileItem->setIsDirectory(true);
            std::vector<LFile> childrenFiles;
            QJsonArray children = jsonObject["children"].toArray();
            for (const auto &child: children) {
                QJsonObject childObj = child.toObject();
                LFile childItem;
                childItem.setFileId(childObj["fileId"].toString());
                childItem.setFileName(childObj["name"].toString());
                childItem.setFileSize(childObj["length"].toVariant().toLongLong());
                childItem.setFileType(FILE_FILE);
                childItem.setIsDirectory(false);
                childrenFiles.push_back(childItem);
            }
            fileItem->setFileList(childrenFiles);
        } else {
            fileItem->setFileType(FILE_FILE);
            fileItem->setIsDirectory(false);
        }
        files.push_back(fileItem);
    }
    auto *fileTransfer = new FileTransferV5();
    fileTransfer->fromDevice = fromDevice;
    fileTransfer->files = files;
    fileTransfer->resumeEnabled = true;

    if (Config::instance().acceptRecvFiles) {
        startReceiveFile(fileTransfer, files, stream, encData, true);
    } else {
        auto *acceptFiles = new AcceptFiles();
        acceptFiles->fileTransfer = fileTransfer;
        acceptFiles->needEncData = encData;
        acceptFiles->device = fromDevice;
        acceptFiles->files = files;
        acceptFiles->stream = stream;
        acceptFiles->tcpClient = std::move(tcpClient);
        acceptFiles->resume = true;
        emit
        LANShareWindow::getInstance()->sigRequestRecvFiles(acceptFiles);
    }
}

void FileServerV5::startReceiveFile(FileTransfer *baseTransfer, std::vector<LFile *> files, CustomDataStream *stream,
                                    bool encData, bool isAgree) {
    auto *fileTransfer = static_cast<FileTransferV5 *>(baseTransfer);
    try {
        stream->writeInt(isAgree ? FS_AGREE : FS_NOT_AGREE);
        if (!isAgree) {
            return;
        }
        // 回传续传偏移量表（fileId -> 已完成字节）
        QString resumeMapJson = buildResumeMapJson(files);
        stream->writeString(resumeMapJson.toUtf8().toStdString());
        callback->onStart(fileTransfer);
        for (LFile *fileItem: files) {
            fileItem->setCustomDataStream(stream);
        }
        for (LFile *fileItem: files) {
            int type = fileItem->getFileType();
            if (type == FILE_FOLDER) {
                handleFolder(stream, fileTransfer, fileItem, encData);
            } else {
                handleFile(stream, fileTransfer, fileItem, encData);
            }
            delete fileItem;
        }
    } catch (const std::exception &e) {
        qDebug() << "FileServerV5::startReceiveFile error: " << e.what();
    }
    delete fileTransfer;
    stream->close();
    delete stream;
}

// ==================== 单文件 ====================

void FileServerV5::handleFile(CustomDataStream *stream, FileTransferV5 *fileTransfer, LFile *fileItem,
                              bool encData) const {
    QString fileId = fileItem->getFileId();
    mlong length = fileItem->getFileSize();
    // 无稳定 fileId：直接落地（V4 行为）
    if (fileId.isEmpty()) {
        receiveDirect(stream, fileTransfer, fileItem, encData);
        return;
    }
    if (length <= 0) {
        QString target = landingPathOf(fileItem->getFileName());
        QFile f(target);
        if (f.open(QIODevice::WriteOnly)) {
            f.close();
        }
        fileItem->setPath(target);
        callback->onFinish(fileTransfer, fileItem, true);
        return;
    }
    QString part = ResumeStore::filePart(fileId);
    mlong offset = ResumeStore::readOffset(part, length);
    mlong received;
    if (offset >= length) {
        received = length;
    } else {
        IOUtils *out = openPart(part, offset);
        received = recvStream(stream, fileTransfer, out, length, fileItem, fileItem, offset, offset, encData);
        out->close();
        delete out;
    }
    if (received >= length) {
        QString finalFile = ResumeStore::finalizePart(part, baseTarget(fileItem->getFileName()));
        fileItem->setPath(finalFile);
        callback->onFinish(fileTransfer, fileItem, true);
    } else {
        callback->onFinish(fileTransfer, fileItem, false);
    }
}

void FileServerV5::receiveDirect(CustomDataStream *stream, FileTransferV5 *fileTransfer, LFile *fileItem,
                                 bool encData) const {
    QString target = landingPathOf(fileItem->getFileName());
    mlong length = fileItem->getFileSize();
    mlong total = 0;
    if (length > 0) {
        IOUtils out(target, QFile::WriteOnly);
        total = recvStream(stream, fileTransfer, &out, length, fileItem, fileItem, 0, 0, encData);
        out.close();
        fileItem->setPath(target);
    } else {
        QFile f(target);
        if (f.open(QIODevice::WriteOnly)) {
            f.close();
        }
        fileItem->setPath(target);
    }
    callback->onFinish(fileTransfer, fileItem, length == total);
}

// ==================== 文件夹 ====================

void FileServerV5::handleFolder(CustomDataStream *stream, FileTransferV5 *fileTransfer, LFile *fileItem,
                                bool encData) const {
    QString folderFileId = fileItem->getFileId();
    if (folderFileId.isEmpty()) {
        handleFolderDirect(stream, fileTransfer, fileItem, encData);
        return;
    }
    mlong fileSize = fileItem->getFileSize();
    std::vector<LFile> children = fileItem->getFileList();
    QString stage = ResumeStore::folderStageDir(folderFileId);
    QString finalDir = ResumeStore::readTarget(stage);
    if (finalDir.isEmpty()) {
        finalDir = mUtils::avoidDuplication(QFileInfo(baseTarget(fileItem->getFileName())));
        QDir().mkpath(finalDir);
        ResumeStore::writeTarget(stage, finalDir);
    }
    mlong total = 0;
    bool failed = false;
    for (auto &child: children) {
        QString childName = child.getFileName();
        QString donePath = finalDir + SEPARATORS + childName;
        if (child.getFileSize() <= 0) {
            QDir().mkpath(QFileInfo(donePath).absolutePath());
            QFile ef(donePath);
            if (ef.open(QIODevice::WriteOnly)) {
                ef.close();
            }
            continue;
        }
        QFileInfo done(donePath);
        QString childPartPath = ResumeStore::childPart(folderFileId, child.getFileId());
        mlong offset = (done.exists() && done.size() == child.getFileSize())
                       ? child.getFileSize()
                       : ResumeStore::readOffset(childPartPath, child.getFileSize());
        mlong received;
        if (offset >= child.getFileSize()) {
            received = child.getFileSize();
        } else {
            IOUtils *out = openPart(childPartPath, offset);
            received = recvStream(stream, fileTransfer, out, fileSize, fileItem, &child, offset, total + offset,
                                  encData);
            out->close();
            delete out;
        }
        if (received < 0) {
            failed = true;
            break;
        }
        if (received >= child.getFileSize()) {
            ResumeStore::movePartToFinal(childPartPath, finalDir, childName);
        }
        total += received;
    }
    bool success = !failed && total == fileSize;
    if (success) {
        ResumeStore::deleteDir(stage);
    }
    fileItem->setPath(finalDir);
    callback->onFinish(fileTransfer, fileItem, success);
}

void FileServerV5::handleFolderDirect(CustomDataStream *stream, FileTransferV5 *fileTransfer, LFile *fileItem,
                                      bool encData) const {
    mlong fileSize = fileItem->getFileSize();
    std::vector<LFile> children = fileItem->getFileList();
    QString folderPath = mUtils::avoidDuplication(QFileInfo(baseTarget(fileItem->getFileName())));
    QDir().mkpath(folderPath);
    mlong total = 0;
    for (auto &child: children) {
        QString childPath = mUtils::avoidDuplication(QFileInfo(folderPath + SEPARATORS + child.getFileName()));
        QDir().mkpath(QFileInfo(childPath).absolutePath());
        if (child.getFileSize() <= 0) {
            QFile f(childPath);
            if (f.open(QIODevice::WriteOnly)) {
                f.close();
            }
            continue;
        }
        mlong received;
        IOUtils out(childPath, QFile::WriteOnly);
        received = recvStream(stream, fileTransfer, &out, fileSize, fileItem, &child, 0, total, encData);
        out.close();
        if (received < 0) {
            total = 0;
            break;
        }
        total += received;
    }
    fileItem->setPath(folderPath);
    callback->onFinish(fileTransfer, fileItem, total == fileSize);
}

// ==================== 流接收核心（带续传偏移） ====================

mlong FileServerV5::recvStream(CustomDataStream *stream, FileTransfer *fileTransfer, IOUtils *out, mlong finalSize,
                               LFile *baseFileItem, const LFile *fileItem, mlong startOffset, mlong progressBase,
                               bool encData) const {
    mlong fileIndex = startOffset;
    mlong sent = 0;
    mlong targetSize = fileItem->getFileSize() - startOffset;
    int progress;
    int lastProgress = 0;
    std::vector<mbyte> buffer(1024 * 1024);
    while (true) {
        int length = stream->readInt();
        if (length <= 0) {
            qDebug("cmd error: %d", length);
            return -1;
        }
        int ten = stream->readFully(buffer.data(), length);
        if (ten <= 0) {
            break;
        }
        if (encData) {
            // 解密：index 为文件内绝对偏移，与发送端加密对齐
            mUtils::decData(buffer.data(), ten, 0, fileIndex);
        }
        out->write(reinterpret_cast<char *>(buffer.data()), ten);
        fileIndex += ten;
        sent += ten;
        targetSize -= ten;
        progress = finalSize > 0 ? static_cast<int>((progressBase + sent) * 100 / finalSize) : 100;
        if (progress != lastProgress) {
            lastProgress = progress;
            baseFileItem->setProgress(progress);
            callback->onProgress(fileTransfer, baseFileItem);
        }
        if (targetSize <= 0) {
            break;
        }
    }
    if (fileIndex != fileItem->getFileSize()) {
        return -1;
    }
    return fileIndex;
}

// ==================== 偏移量表构建 ====================

QString FileServerV5::buildResumeMapJson(const std::vector<LFile *> &files) {
    QJsonObject map;
    const QString root = ResumeStore::partRoot();
    for (LFile *fileItem: files) {
        QString fileId = fileItem->getFileId();
        if (fileId.isEmpty()) {
            continue;
        }
        if (fileItem->getFileType() == FILE_FOLDER) {
            QString stage = root + SEPARATORS + fileId;
            if (!QDir(stage).exists()) {
                continue;
            }
            QString finalDir = ResumeStore::readTarget(stage);
            std::vector<LFile> children = fileItem->getFileList();
            for (const auto &child: children) {
                if (child.getFileSize() <= 0 || child.getFileId().isEmpty()) {
                    continue;
                }
                mlong off;
                QString donePath = finalDir.isEmpty() ? QString() : finalDir + SEPARATORS + child.getFileName();
                QFileInfo done(donePath);
                if (!donePath.isEmpty() && done.exists() && done.size() == child.getFileSize()) {
                    off = child.getFileSize();
                } else {
                    off = ResumeStore::readOffset(ResumeStore::childPart(fileId, child.getFileId()),
                                                  child.getFileSize());
                }
                if (off > 0) {
                    map[child.getFileId()] = off;
                }
            }
        } else {
            mlong length = fileItem->getFileSize();
            if (length <= 0) {
                continue;
            }
            mlong off = ResumeStore::readOffset(ResumeStore::filePart(fileId), length);
            if (off > 0) {
                map[fileId] = off;
            }
        }
    }
    return QString::fromUtf8(QJsonDocument(map).toJson(QJsonDocument::Compact));
}

// ==================== 辅助方法 ====================

IOUtils *FileServerV5::openPart(const QString &part, mlong startOffset) {
    mUtils::createMultipleFolders(QFileInfo(part).absolutePath());
    QFile::OpenMode mode = startOffset > 0
                           ? static_cast<QFile::OpenMode>(QIODevice::WriteOnly | QIODevice::Append)
                           : QFile::WriteOnly;
    return new IOUtils(part, mode);
}

QString FileServerV5::saveDir() {
    return QDir::cleanPath(Config::instance().saveFilePath);
}

QString FileServerV5::baseTarget(const QString &name) {
    return saveDir() + SEPARATORS + name;
}

QString FileServerV5::landingPathOf(const QString &name) {
    mUtils::createMultipleFolders(saveDir());
    return mUtils::avoidDuplication(QFileInfo(baseTarget(name)));
}
