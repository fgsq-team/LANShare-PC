//
// Created by 16508 on 2025/8/30.
//

#include "FileServer.h"

#include "CustomDataStream.h"
#include "FileTransfer.h"
#include "LANShare.h"
#include "LANShareWindow.h"
#include "LFile.h"
#include "ProgressCallback.h"


// 具体实现
class FileProgressCallback : public ProgressCallback {
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
                true,
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
            qDebug("recv fail");
            emit
            LANShareWindow::getInstance()->sigRecviceFileSuccess(
                fileItem->getUuid(), false,
                ""
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
                true,
                false,
                false
            );
        }
    }
};

FileServer::FileServer() {
    callback = new FileProgressCallback();
}

void FileServer::handleFileTransfer(const Device &fromDevice, std::unique_ptr<TCPClient> tcpClient,
                                    CustomDataStream *stream) {
    try {
        bool encData = stream->readBoolean();
        std::string json = stream->readString();
        QJsonDocument doc = QJsonDocument::fromJson(json.c_str());
        QJsonObject data = doc.object();
        QJsonArray filesJsonArray = data["files"].toArray();
        std::vector<LFile *> files;
        for (const auto &fileJson: filesJsonArray) {
            auto jsonObject = fileJson.toObject();
            int fileType = jsonObject["fileType"].toInt();
            mlong fileSize = jsonObject["length"].toVariant().toLongLong();
            QString fileName = jsonObject["name"].toString();
            QString fileId = jsonObject["fileId"].toString();
            auto fileItem = new LFile();
            fileItem->setFileSize(fileSize);
            fileItem->setFileName(fileName);
            fileItem->setFileId(fileId);
            if (fileType == FILE_TYPE_FOLDER) {
                std::vector<LFile> childrenFiles;
                fileItem->setFileType(FILE_FOLDER);
                QJsonArray children = jsonObject["children"].toArray();
                for (const auto &child: children) {
                    jsonObject = child.toObject();
                    fileType = jsonObject["fileType"].toInt();
                    fileSize = jsonObject["length"].toVariant().toLongLong();
                    fileName = jsonObject["name"].toString();
                    LFile childrenFileItem;
                    childrenFileItem.setFileSize(fileSize);
                    childrenFileItem.setFileName(fileName);
                    childrenFileItem.setFileType(fileType);
                    childrenFiles.push_back(childrenFileItem);
                }
                fileItem->setFileList(childrenFiles);
            } else {
                fileItem->setFileType(FILE_FILE);
            }
            files.push_back(fileItem);
        }
        auto *fileTransfer = new FileTransfer();
        fileTransfer->files = files;
        fileTransfer->fromDevice = fromDevice;

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
            emit
            LANShareWindow::getInstance()->sigRequstRecvFiles(acceptFiles);
        }
    } catch (const std::exception &e) {
        qDebug() << "file receive error: " << e.what();
    }
}

void FileServer::handleVersion1(const Device &device, std::unique_ptr<TCPClient> tcpClient) {
    auto stream = new CustomDataStream(tcpClient.get());
    int cmd = stream->readInt();
    switch (cmd) {
        case FS_SHARE_FILE: // 文件传输
            handleFileTransfer(device, std::move(tcpClient), stream);
            break;
        case FS_MESSAGE: // 接收消息
            handleMessage(device, stream);
            break;
        case FS_GET_NO_SYNC_MEDIA: // 媒体同步
            handleMediaSync(device, stream);
            break;
        default:
            break;
    }
}

// 暂时不支持媒体同步
void FileServer::handleMediaSync(const Device &device, CustomDataStream *stream) {
    stream->close();
}

void FileServer::handleMessage(const Device &device, CustomDataStream *stream) {
    bool isClip = stream->readBoolean();
    QString messageEnc = QString::fromUtf8(stream->readString().c_str());
    try {
        const QString message = mUtils::decMessage(messageEnc, Config::instance().messageKey);
        qDebug() << "devName:" << device.getDevName() << " message:" << message;
        emit
        LANShareWindow::getInstance()->sigNewMessage(device, message, true);
        if (isClip) {
            emit
            LANShareWindow::getInstance()->sigCopyText(message);
        }
        qDebug("client close");
    } catch (const std::exception &e) {
        qDebug() << "message dec error" << e.what();
    }
    stream->close();
}

void FileServer::startReceiveFile(FileTransfer *fileTransfer, std::vector<LFile *> files,
                                  CustomDataStream *stream, boolean encData, boolean isAgree) {
    try {
        stream->writeInt(isAgree ? FS_AGREE : FS_NOT_AGREE);
        if (!isAgree) return;
        callback->onStart(fileTransfer);
        for (LFile *fileItem: files) {
            fileItem->setCustomDataStream(stream);
            int type = fileItem->getFileType();
            if (type == FILE_FOLDER) {
                handleFolder(stream, fileTransfer, fileItem, encData);
            } else {
                handleFile(stream, fileTransfer, fileItem, encData);
            }
            delete fileItem;
        }
    } catch (const std::exception &e) {
        qDebug() << "startReceiveFile error: " << e.what();
    }
    delete fileTransfer;
    stream->close();
    delete stream;
}

void FileServer::handleFolder(CustomDataStream *stream, FileTransfer *fileTransfer, LFile *fileItem,
                              boolean encData) const {
    mlong fileSize = fileItem->getFileSize();
    auto children = fileItem->getFileList();
    mlong total = 0;
    QString saveFilePath = QDir::cleanPath(Config::instance().saveFilePath);
    QString folderPath = saveFilePath + SEPARATORS + fileItem->getFileName();
    mUtils::createMultipleFolders(saveFilePath);
    QFileInfo folderFile(folderPath);
    folderPath = mUtils::avoidDuplication(folderFile);
    for (auto &child: children) {
        QString childPath = folderPath + SEPARATORS + child.getFileName();
        QFileInfo f(childPath);
        QDir dir = f.dir();
        mUtils::createMultipleFolders(dir.absolutePath());
        if (child.getFileSize() <= 0) {
            // 创建空文件
            QFile file(childPath);
            if (!file.open(QIODevice::WriteOnly)) {
                file.close();
            }
            continue;
        }
        mlong ten;
        if (encData) {
            ten = recvStreamToFileDec(fileTransfer, stream, total, fileSize, fileItem, &child, childPath);
        } else {
            ten = recvStreamToFile(fileTransfer, stream, total, fileSize, fileItem, &child, childPath);
        }
        if (ten < 0) {
            total = 0;
            break;
        }
        total += ten;
    }
    fileItem->setPath(folderPath);
    callback->onFinish(fileTransfer, fileItem, fileItem->getFileSize() == total);
}

void FileServer::handleFile(CustomDataStream *stream, FileTransfer *fileTransfer, LFile *fileItem, boolean encData) {
    auto fileName = fileItem->getFileName();
    mlong fileSize = fileItem->getFileSize();
    QString saveFilePath = QDir::cleanPath(Config::instance().saveFilePath);
    mUtils::createMultipleFolders(saveFilePath);
    QString filePath = saveFilePath + SEPARATORS + fileName;
    QFileInfo file(filePath);
    filePath = mUtils::avoidDuplication(file);
    qDebug() << "fileName: " << fileName << "fileSize: " << fileSize;
    qDebug() << "saveFilePath:" << filePath;
    try {
        mlong total = 0;
        if (fileSize <= 0) {
            // 创建空文件
            QFile emptyFile(filePath);
            if (!emptyFile.open(QIODevice::WriteOnly)) {
                emptyFile.close();
            }
        } else {
            if (encData) {
                total = recvStreamToFileDec(fileTransfer, stream, 0, fileSize, fileItem, fileItem, filePath);
            } else {
                total = recvStreamToFile(fileTransfer, stream, 0, fileSize, fileItem, fileItem, filePath);
            }
        }
        fileItem->setPath(filePath);
        callback->onFinish(fileTransfer, fileItem, fileItem->getFileSize() == total);
    } catch (const std::exception &e) {
        qDebug() << "FileServer::handleFile error: " << e.what();
    }
}


mlong FileServer::recvStreamToFile(
    FileTransfer *fileTransfer, CustomDataStream *stream, mlong total, mlong finalSize, LFile *baseFile,
    const LFile *fileItem, const QString &filePath
) const {
    int ten;
    mlong subTotal = 0;
    mlong targetSize = fileItem->getFileSize();
    int progress = 0;
    int lastProgress = 0;
    auto buffer = new mbyte[1024 * 1024];
    auto *outFileStream = new IOUtils(filePath, QFile::WriteOnly);
    while (true) {
        int32_t length = stream->readInt();
        if (length <= 0) {
            break;
        }
        ten = stream->readFully(buffer, length);
        if (ten <= 0) {
            break;
        }
        outFileStream->write(reinterpret_cast<char *>(buffer), ten);
        subTotal += ten;
        total += ten;
        targetSize -= ten;
        progress = static_cast<int>(total * 100 / finalSize);
        if (progress > lastProgress) {
            lastProgress = progress;
            baseFile->setProgress(progress);
            callback->onProgress(fileTransfer, baseFile);
        }
        if (targetSize <= 0) {
            break;
        }
    }
    outFileStream->close();
    if (subTotal != fileItem->getFileSize()) {
        subTotal = -1;
    }
    delete[] buffer;
    return subTotal;
}


mlong FileServer::recvStreamToFileDec(
    FileTransfer *fileTransfer, CustomDataStream *stream, mlong total, mlong finalSize, LFile *baseFile,
    const LFile *fileItem, const QString &filePath
) const {
    int ten;
    mlong subTotal = 0;
    mlong targetSize = fileItem->getFileSize();
    int progress = 0;
    int lastProgress = 0;
    auto buffer = new mbyte[1024 * 1024];
    auto *outFileStream = new IOUtils(filePath, QFile::WriteOnly);
    while (true) {
        int32_t length = stream->readInt();
        if (length <= 0) {
            break;
        }
        ten = stream->readFully(buffer, length);
        if (ten <= 0) {
            break;
        }
        mUtils::decData(buffer, ten, 0, subTotal);
        outFileStream->write(reinterpret_cast<char *>(buffer), ten);
        subTotal += ten;
        total += ten;
        targetSize -= ten;
        progress = static_cast<int>(total * 100 / finalSize);
        if (progress > lastProgress) {
            lastProgress = progress;
            baseFile->setProgress(progress);
            callback->onProgress(fileTransfer, baseFile);
        }
        if (targetSize <= 0) {
            break;
        }
    }
    outFileStream->close();
    if (subTotal != fileItem->getFileSize()) {
        subTotal = -1;
    }
    delete[] buffer;
    return subTotal;
}
