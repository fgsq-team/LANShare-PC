//
// Created by fgsqme on 2024/5/7.
//

#include "LHttpServer.h"
#include "Config.hpp"
#include "TokenDBUtil.h"
#include "LWebSocketServer.h"
#include "StringLockManager.h"
#include "LANShare.h"
#include "ContentTypes.h"
#include "ThumbnailUtil.h"
#include "zip.h"
#include "Utils.h"
#include <QUrl>

#if defined(PLATFORM_WINDOWS)
#define DEFAULT_WEB_ROOT_FILE_PATH  "/"
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#define DEFAULT_WEB_ROOT_FILE_PATH  QDir::homePath()
#endif

struct MediaFile {
    int index = 0;
    QString mediaPath;
};

struct MediaFolder {
    QString folderPath;
    QVector<MediaFile> mediaFiles;
};

int SEND_MSSAGE = 1;
int SYNC_DEVICE_LIST = 2;
int CHANGE_THEME = 3;  // WebSocket 主题变更命令标识
// Paths
QStringList paths = {
        "/apps",
        "/media",
        "/files",
        "/compressMedias",
        "/apkfile/*",
        "/file/*",
        "/wss",
        "/imageload/*",
        "/chatUploadFile",
        "/uploadFile"
};

std::vector<LWebSocketServer *> websockets;
QMap<int, MediaFolder> mediaFolders;
QMap<int, QString> allMedias;

// 递归扫描文件夹下的所有图片文件
void scanMovies(const QString &folderPath, QMap<QString, QStringList> &cacheMap) {
    QDir dir(folderPath);
    // 列出文件夹下的所有文件
    QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            foreach(
            const QFileInfo &fileInfo, fileInfoList) {
            QString name = fileInfo.suffix().toLower();
            if (fileInfo.isDir()) {
                // 如果是文件夹，递归调用自己
                scanMovies(fileInfo.absoluteFilePath(), cacheMap);
            } else if (
                    name == "jpg"
                    || name == "png"
                    || name == "bmp"
                    || name == "jpeg"
//                    || name == "mp4"
                    ) {
                // 如果是图片文件，则将其添加到相应文件夹的列表中
                QString folderName = fileInfo.dir().absolutePath();
                cacheMap[folderName] << fileInfo.absoluteFilePath();
            }
        }
}

void scanImages() {
    std::thread([]() {
        // 获取图库目录列表
        QStringList picturesDirs = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
        QStringList moviesDirs = QStandardPaths::standardLocations(QStandardPaths::MoviesLocation);
        QMap<QString, QStringList> cacheMap;
        // 输出图库目录列表
        for (const QString &dir: moviesDirs) {
            scanMovies(dir, cacheMap);
        }
        for (const QString &dir: picturesDirs) {
            scanMovies(dir, cacheMap);
        }
        int forderIndex = 0;
        int index = 0;
        // 输出结果
        QMapIterator<QString, QStringList> imageMapIterator(cacheMap);
        while (imageMapIterator.hasNext()) {
            imageMapIterator.next();
            MediaFolder mediaFolder;
            mediaFolder.folderPath = imageMapIterator.key();
            for (auto &media: imageMapIterator.value()) {
                MediaFile mediaFile;
                mediaFile.mediaPath = media;
                mediaFile.index = index;
                allMedias[index++] = media;
                mediaFolder.mediaFiles.append(mediaFile);
            }
            mediaFolders[forderIndex++] = mediaFolder;
        }
    }).detach();
}

/**
 * 向所有 WebSocket 客户端推送主题变更通知
 * 消息格式：{"cmd": 3, "theme": "xxx"}
 */
QJsonArray LHttpServer::webMenus;

void LHttpServer::sendTheme() {
    std::mutex &websocket_mutex = StringLockManager::getStringLock("websockets");
    websocket_mutex.lock();
    auto it = websockets.begin();
    while (it != websockets.end()) {
        if ((*it)->isClosed()) {
            delete (*it);
            it = websockets.erase(it);
        } else {
            QJsonObject jsonObject;
            jsonObject["cmd"] = CHANGE_THEME;
            // 将内部主题名称映射为网页端主题枚举
            QString themeName = Config::instance().themeName;
            if (themeName.isEmpty()) {
                themeName = "follow_system";
            }
            jsonObject["theme"] = themeName;
            QJsonDocument rdoc;
            rdoc.setObject(jsonObject);
            QByteArray jsonString = rdoc.toJson(QJsonDocument::Compact);
            (*it)->sendString(jsonString);
            ++it;
        }
    }
    websocket_mutex.unlock();
}

void LHttpServer::sendDeviceList() {
    std::mutex &websocket_mutex = StringLockManager::getStringLock("websockets");
    websocket_mutex.lock();
    auto it = websockets.begin();
    while (it != websockets.end()) {
        if ((*it)->isClosed()) {
            // 删除满足条件的对象
            delete (*it);
            it = websockets.erase(it);
        } else {
            // 继续迭代
            QJsonObject jsonObject;
            jsonObject["cmd"] = SYNC_DEVICE_LIST;
            QJsonArray array;
            for (auto const &[key, value]: LANShare::getInstance()->getOnLineDevices()) {
                QJsonObject json;
                json["devName"] = value.getDevName();
                json["devIP"] = value.getDevIp();
                json["devPort"] = value.getDevPort();
                json["devMode"] = value.getDevMode();
                json["dataVersion"] = value.getDataVersion();
                json["address"] = value.getDevIp() + ":" + QString::number(value.getDevPort());
                array.append(json);
            }
            jsonObject["data"] = array;
            QJsonDocument rdoc;
            rdoc.setObject(jsonObject);
            QByteArray jsonString = rdoc.toJson(QJsonDocument::Compact);
            (*it)->sendString(jsonString);
            ++it;
        }
    }
    websocket_mutex.unlock();
}

void LHttpServer::sendWebSocketMessage(
        const QString &message, const QString &toDevName, const QString &filePath,
        int messageType, int devType, const QString &fileSize,
        bool isLeft, bool isClip, bool isFile
) {
    std::mutex &websocket_mutex = StringLockManager::getStringLock("websockets");
    websocket_mutex.lock();
    auto it = websockets.begin();
    while (it != websockets.end()) {
        if ((*it)->isClosed()) {
            // 删除满足条件的对象
            delete (*it);
            it = websockets.erase(it);
        } else {
            // 继续迭代
            QJsonObject jsonObject;
            jsonObject["cmd"] = SEND_MSSAGE;
            jsonObject["isLeft"] = isLeft;
            jsonObject["message"] = message;
            jsonObject["devName"] = toDevName;
            jsonObject["devType"] = devType;
            jsonObject["messageType"] = messageType;
            jsonObject["isClip"] = isClip;
            jsonObject["filePath"] = filePath;
            jsonObject["fileSize"] = fileSize;
            jsonObject["isFile"] = isFile;
            QJsonDocument rdoc;
            rdoc.setObject(jsonObject);
            QByteArray jsonString = rdoc.toJson(QJsonDocument::Compact);
            (*it)->sendString(jsonString);
            ++it;
        }
    }
    websocket_mutex.unlock();
}


void listDirectory(const QDir &dir, QJsonArray &jsonArray) {
    // 获取目录中的所有项，包括文件和文件夹，但不包括 "." 和 ".."
    QFileInfoList fileList = dir.entryInfoList((QDir::NoDotAndDotDot | QDir::AllEntries));
    // 按名称排序，并确保文件夹在前，文件在后
    std::sort(fileList.begin(), fileList.end(), [](const QFileInfo &a, const QFileInfo &b) {
        if (a.isDir() && !b.isDir()) return true; // 文件夹优先
        if (!a.isDir() && b.isDir()) return false; // 文件在后
        return a.fileName().compare(b.fileName()) < 0; // 按名称排序
    });
    // 遍历排序后的文件和文件夹
            foreach(
            const QFileInfo &fileInfo, fileList) {
            QJsonObject jsonObject;
            jsonObject["isDirectory"] = fileInfo.isDir();
            jsonObject["isFile"] = fileInfo.isFile();
            jsonObject["length"] = fileInfo.isDir() ? 0 : fileInfo.size(); // 仅对文件设置长度
            jsonObject["name"] = fileInfo.fileName();
            QString path = fileInfo.absoluteFilePath();
            jsonObject["path"] = path;
            // 获取文件的最后修改时间，并转换为JSON格式的时间字符串
            QDateTime lastModified = fileInfo.lastModified();
            jsonObject["time"] = lastModified.toString("yyyy-MM-dd HH:mm:ss");
            jsonArray.append(jsonObject);
        }
}

void composeZip(zip_t *zip, int pathPrefix, const QString &path) {
    QFileInfo info(path);
    if (info.isFile()) {
        QString name1 = path.mid(pathPrefix);
        qDebug() << "name1:" << name1;
        zip_entry_open(zip, name1.toUtf8().constData());
        {
            char buf[1024];
            IOUtils ioUtils(info.absoluteFilePath(), QFile::ReadOnly);
            int ten = 0;
            while ((ten = ioUtils.read(buf, 1024)) > 0) {
                zip_entry_write(zip, buf, ten);
            }
            ioUtils.close();
        }
        zip_entry_close(zip);
    } else if (info.isDir()) {
        // 列出文件夹下的所有文件
        QFileInfoList fileInfoList = QDir(info.absoluteFilePath()).entryInfoList(
                QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &fileInfo: fileInfoList) {
            composeZip(zip, pathPrefix, fileInfo.absoluteFilePath());
        }
    }
}

LHttpServer::LHttpServer(LANShare *lanShare) : lanShare(lanShare) {
    // webMenus.append(QJsonObject{{"key", "apps"},    {"text", "软件"},     {"icon", "nav-item-media-apps"}});
    webMenus.append(QJsonObject{{"key", "media"},   {"text", "图片"},     {"icon", "nav-item-media-img"}});
    webMenus.append(QJsonObject{{"key", "files"},   {"text", "文件列表"}, {"icon", "nav-item-media-folder"}});
    webMenus.append(QJsonObject{{"key", "chat"},    {"text", "消息记录"}, {"icon", "nav-item-media-record"}});
    // webMenus.append(QJsonObject{{"key", "draw"},    {"text", "远程绘图"}, {"icon", "nav-item-media-record"}});
    scanImages();
    httpServer = std::make_unique<HttpServer>();
    httpServer->setRequestFilter([](Request *request, Response *response, HttpHandler httpHandler) {
        if (Config::instance().openWebService) {
            httpHandler(request, response);
            return;
        }
        for (const auto &path: paths) {
            if (HttpServer::pathMatches(path, request->getRequestURL())) {
                QString token;
                if (request->getRequestMethod() == "POST") {
                    token = request->getHeaderValue("token");
                } else {
                    token = request->getPathParam("token");
                }
//                qDebug() << "token:" << token;
                if (token.isEmpty()) {
                    response->write302("访问权限已失效，请刷新主页后授权", "/");
                    return;
                }
                TokenDBUtil &tokenDBUtil = TokenDBUtil::instance();
                QString s = tokenDBUtil.queryToken(token);
                if (s.isEmpty()) {
                    response->write302("访问权限已失效，请刷新主页后授权", "/");
                    return;
                }
            }
        }
        httpHandler(request, response);
    });

    httpServer->addPath("/hello", [](Request *request, Response *response) {
        response->writeString("ok", Response::TEXT_CONTEXT_TYPE);
    });

    httpServer->addPath("/file/*", [](Request *request, Response *response) {
        QString path = request->getPathParam("path");
        qDebug() << "path:" << path;
        response->writeFile(path);
    });

    httpServer->addPath("/files", [](Request *request, Response *response) {
        QString str = request->getRequestBody();
        QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
        QJsonObject object = doc.object();
        QString path = object["path"].toString();
//        QUrl decodedUrl = QUrl::fromPercentEncoding(path.toUtf8());
//        path = decodedUrl.toString();
        qDebug() << "path:" << path;
        bool isBack = object["isBack"].toBool();
        QDir dir(path);
        if (isBack) {
            bool d = dir.cdUp();
            if (d) {
                path = dir.absolutePath();
            } else {
                path = DEFAULT_WEB_ROOT_FILE_PATH;
                dir = QDir(path);
            }
        }
        QJsonArray reusult;
        QJsonObject upJsonObject;
        upJsonObject["isDirectory"] = true;
        upJsonObject["isFile"] = false;
        upJsonObject["length"] = 0; // 仅对文件设置长度
        upJsonObject["name"] = "...";
        upJsonObject["path"] = path;
        // 获取文件的最后修改时间，并转换为JSON格式的时间字符串
        upJsonObject["time"] = 0;
        reusult.append(upJsonObject);
#if defined(PLATFORM_WINDOWS)
        if (path == DEFAULT_WEB_ROOT_FILE_PATH) {
            QList<QFileInfo> drives = QDir::drives();
                    foreach(const QFileInfo &drive, drives) {
                    QJsonObject jsonObject;
                    jsonObject["isDirectory"] = true;
                    jsonObject["isFile"] = false;
                    jsonObject["length"] = 0; // 仅对文件设置长度
                    jsonObject["name"] = drive.absoluteFilePath().left(2);
                    jsonObject["path"] = drive.absoluteFilePath();
                    QDateTime lastModified = drive.lastModified();
                    jsonObject["time"] = lastModified.toString("yyyy-MM-dd HH:mm:ss");
                    reusult.append(jsonObject);
                }
        } else {
            listDirectory(dir, reusult);
        }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
        listDirectory(dir, reusult);
#endif
        QJsonObject qJsonObject;
        qJsonObject["list"] = reusult;
        qJsonObject["path"] = path;
        QJsonDocument rdoc;
        rdoc.setObject(qJsonObject);
        QByteArray jsonString = rdoc.toJson(QJsonDocument::Compact);
        response->writeString(jsonString.data());
    });

    httpServer->addPath("/checkPass", "POST", [](Request *request, Response *response) {
        QString token = request->getHeaderValue("token");
        TokenDBUtil &tokenDBUtil = TokenDBUtil::instance();
        QString s = tokenDBUtil.queryToken(token);
        QJsonObject object;
        object["pass"] = !s.isEmpty();
        QJsonDocument jsonDocument(object);
        QString jsonString = jsonDocument.toJson(QJsonDocument::Compact);
//        qDebug() << "token:" << token;
        response->writeString(jsonString);
    });

    httpServer->addPath("/initConfig", [](Request *request, Response *response) {
        QJsonObject object;
        object["rootPath"] = DEFAULT_WEB_ROOT_FILE_PATH;
        // 添加当前主题配置
        QString themeName = Config::instance().themeName;
        if (themeName.isEmpty()) {
            themeName = "follow_system";
        }
        object["theme"] = themeName;
        object["menus"] = webMenus;

        QString token = request->getHeaderValue("token");
        bool flag = false;
        TokenDBUtil &tokenDBUtil = TokenDBUtil::instance();
        Token t = tokenDBUtil.queryCustomIp(request->getClientIP());
        if (!t.isNull()) {
            token = t.getToken();
            flag = true;
        } else {
            if (token.isNull()) {
                token = Utils::getUUID();
                if (!Config::instance().openWebService) {
                    emit
                    LANShare::getInstance()->mainWindow->sigNewWebClient(token, request->getClientIP());
                } else {
                    flag = true;
                }
                response->addHeader("token", token);
            } else {
                QString s = tokenDBUtil.queryToken(token);
                if (s.isNull()) {
                    token = Utils::getUUID();
                    if (!Config::instance().openWebService) {
                        emit
                        LANShare::getInstance()->mainWindow->sigNewWebClient(token, request->getClientIP());
                    } else {
                        flag = true;
                    }
                } else {
                    flag = true;
                }
            }
        }
        object["token"] = token;
        object["pass"] = flag;
        object["name"] = Config::instance().clientName;
        QJsonDocument jsonDocument(object);
        QString jsonString = jsonDocument.toJson(QJsonDocument::Compact);
        response->writeString(jsonString);
    });

    httpServer->addPath("/wss", [](Request *request, Response *response) {
        auto *webSocketServer = new LWebSocketServer(request, response);
        websockets.push_back(webSocketServer);
        qDebug() << "webSocketServer size: " << QString::number(websockets.size());

        LHttpServer::sendDeviceList();
        while (!webSocketServer->isClosed()) {
            QByteArray frame = webSocketServer->readFrame();
            if (frame.isEmpty()) {
                break;
            }
            QJsonObject jsonObject = QJsonDocument::fromJson(frame).object();
            int cmd = jsonObject.value("cmd").toInt();
            if (cmd == SEND_MSSAGE) {
                QString message = jsonObject.value("message").toString();
                QString selectedDevice = jsonObject.value("selectedDevice").toString();
                bool isClip = jsonObject.value("isClip").toBool();
                QString userName = "全部设备";
                if (selectedDevice.isEmpty()) {
                    LANShare::getInstance()->udpProtocol.broadcastMessage(nullptr, message, isClip, false);
                } else {
                    std::string selectedDev = selectedDevice.toStdString();
                    std::map<std::string, Device> devices = LANShare::getInstance()->getOnLineDevices();
                    if (devices.count(selectedDev) > 0) {
                        Device device = devices[selectedDev];
                        LANShare::getInstance()->udpProtocol.broadcastMessage(&device, message, isClip, false);
                        userName = device.getDevName();
                    }
                }
                userName = userName + " <- " + request->getClientIP();
                Device device;
                device.setDevName(userName);
                device.setDevMode(Device::L_WIN);
                emit
                LANShare::getInstance()->mainWindow->sigNewMessage(device,message, false);
            }
        }
        std::mutex &websocket_mutex = StringLockManager::getStringLock("websockets");
        websocket_mutex.lock();
        websockets.erase(std::remove_if(websockets.begin(), websockets.end(), [webSocketServer](LWebSocketServer *ptr) {
            return ptr == webSocketServer;
        }), websockets.end());
        websocket_mutex.unlock();
        webSocketServer->close();
        delete webSocketServer;
    });

    // 下载压缩后的文件
    httpServer->addPath("/downloadZipFile", [](Request *request, Response *response) {
        QString tempFile = request->getPathParam("tempFile");
        QFile file(Config::instance().saveFilePath + CACHE_DIR + "/" + tempFile);
        QString fileName = QFileInfo(file).fileName();
        response->addHeader("Content-Disposition",
                            "attachment; filename=\"" + fileName + "\"; filename*=UTF-8''" +
                            QUrl::toPercentEncoding(fileName));
        response->writeFile(file);
        file.remove();
    });

    httpServer->addPath("/compressMedias", "POST", [](Request *request, Response *response) {
        QString requestBody = request->getRequestBody();
        QJsonDocument doc = QJsonDocument::fromJson(requestBody.toUtf8());
        QJsonObject object = doc.object();
        QJsonArray list = object["list"].toArray();
        QString fileName = Utils::getUUID() + ".zip";
        QString composeZipPath = Config::instance().saveFilePath + CACHE_DIR + "/" + fileName;
        struct zip_t *zip = zip_open(composeZipPath.toUtf8().constData(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
        for (auto &&i: list) {
            QJsonObject data = i.toObject();
            bool isDirectory = data.value("isDirectory").toBool();
            int index = data.value("index").toInt();
            if (isDirectory) {
                MediaFolder &mediaFolder = mediaFolders[index];
                for (const auto &mediaFile: mediaFolder.mediaFiles) {
                    QDir dir(mediaFolder.folderPath);
                    dir.cdUp();
                    composeZip(zip, dir.absolutePath().length() + 1, mediaFile.mediaPath);
                }
            } else {
                QFileInfo fileInfo(allMedias[index]);
                composeZip(zip, fileInfo.dir().absolutePath().length() + 1, fileInfo.absoluteFilePath());
            }
        }
        zip_close(zip);
        QJsonObject jsonObject;
        jsonObject["tempFile"] = fileName;
        QJsonDocument rdoc;
        rdoc.setObject(jsonObject);
        QByteArray jsonString = rdoc.toJson(QJsonDocument::Compact);
        response->writeString(jsonString.data());
    });

    httpServer->addPath("/compressFiles", "POST", [](Request *request, Response *response) {
        QString requestBody = request->getRequestBody();
        QJsonDocument doc = QJsonDocument::fromJson(requestBody.toUtf8());
        QJsonObject object = doc.object();
        QJsonArray list = object["list"].toArray();
        QString fileName = Utils::getUUID() + ".zip";
        QString composeZipPath = Config::instance().saveFilePath + CACHE_DIR + "/" + fileName;
        struct zip_t *zip = zip_open(composeZipPath.toUtf8().constData(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
        for (auto &&i: list) {
            QString path = i.toString();
            QFileInfo fileInfo(path);
            if (!fileInfo.exists()) {
                qDebug() << "路径不存在 path: " << path;
                response->write500("路径不存在");
                return;
            }
            if (fileInfo.isFile()) {
                composeZip(zip, fileInfo.dir().absolutePath().length(), path);
            } else if (fileInfo.isDir()) {
                composeZip(zip, fileInfo.absolutePath().length(), path);
            } else {
                qDebug() << "不支持的文件类型：" << path;
                response->write500("不支持的文件类型");
                return;
            }
        }
        zip_close(zip);
        QJsonObject jsonObject;
        jsonObject["tempFile"] = fileName;
        QJsonDocument rdoc;
        rdoc.setObject(jsonObject);
        QByteArray jsonString = rdoc.toJson(QJsonDocument::Compact);
        response->writeString(jsonString.data());
    });

    httpServer->addPath("/uploadFile", "POST", [](Request *request, Response *response) {
        QString path = Config::instance().saveFilePath + "网页收到的文件/";
        QDir file(path);
        if (!file.exists()) {
            mUtils::createMultipleFolders(file.path());
        }
        UploadResult uploadResult = request->readUploadBody2Stream(path);
        qint64 fileSize = uploadResult.getFileSize();
        auto *lfile = new LFile();
        lfile->setFileName(uploadResult.getFileName());
        lfile->setFileSize(fileSize);
        lfile->setIsDirectory(false);
        lfile->setUuid(Utils::getUUID());
        lfile->setPath(uploadResult.getFilePath());
        qDebug() << "upload path: " << uploadResult.getFilePath();
        emit
        LANShareWindow::getInstance()->sigRecviceFile(
                lfile,
                lfile->getUuid(),
                uploadResult.getFileName(),
                "网页设备",
                fileSize,
                true,
                !lfile->isDirectory(),
                true
        );
        response->writeString(QString("文件上传成功，大小: ") + Utils::computeSize(fileSize).c_str());
    });

    httpServer->addPath("/chatUploadFile", "POST", [](Request *request, Response *response) {
        QString address = request->getPathParam("address");
        if (address.isEmpty()) {
            response->write500("所选设备不存在或者不在线");
            return;
        }
        std::string addr = address.toStdString();
        std::map<std::string, Device> devices = LANShare::getInstance()->getOnLineDevices();
        if (devices.count(addr) > 0) {
            Device device = devices[addr];
            std::vector<LFile *> selectFiles;
            UploadInputStream uploadInputStream = request->getSingleUploadInputStream();
            auto *file = new LFile();
            file->setType(LFile::STREAM);
            file->setIoInter(&uploadInputStream);
            file->setFileSize(uploadInputStream.getFileSize());
            file->setFileName(uploadInputStream.getFileName());
            file->setIsDirectory(false);
            selectFiles.push_back(file);
            LANShare::getInstance()->legacyFileTransfer.sendFile(device, selectFiles, 1);
            response->writeString(
                    QString("文件上传成功，大小: ") + Utils::computeSize(uploadInputStream.getFileSize()).c_str());
            return;
        }
        response->write500("文件上传失败");
    });

    httpServer->addPath("/media", "POST", [](Request *request, Response *response) {
        QString str = request->getRequestBody();
        QJsonDocument doc = QJsonDocument::fromJson(str.toUtf8());
        QJsonObject object = doc.object();
        int folderIndex = object["folderIndex"].toInt();
//        qDebug() << "folderIndex  " << folderIndex;
        QJsonArray jsonArray;
        if (folderIndex == -1) {
            for (auto iter = mediaFolders.begin(); iter != mediaFolders.end(); ++iter) {
                const int index = iter.key();
                const MediaFolder &folder = iter.value();
                const QDir dir(folder.folderPath);
                const QVector<MediaFile> &images = folder.mediaFiles;
                QJsonObject jsonObject;
                jsonObject["name"] = dir.dirName() + "(" + QString::number(images.size()) + ")";
                jsonObject["path"] = images[0].mediaPath;
                jsonObject["isDirectory"] = true;
                jsonObject["count"] = images.size();
                jsonObject["index"] = index;
                jsonObject["imgIndex"] = images[0].index;
                jsonObject["isVideo"] = false;
                jsonArray.append(jsonObject);
            }
        } else {
            QVector<MediaFile> images = mediaFolders[folderIndex].mediaFiles;
            for (const MediaFile &image: images) {
                const QDir dir(image.mediaPath);
                QJsonObject jsonObject;
                jsonObject["name"] = dir.dirName();
                jsonObject["path"] = dir.absolutePath();
                jsonObject["isDirectory"] = false;
                jsonObject["count"] = 0;
                jsonObject["index"] = image.index;
                jsonObject["imgIndex"] = image.index;
                jsonObject["isVideo"] = false;
                jsonArray.append(jsonObject);
            }
        }
        QJsonDocument rdoc;
        rdoc.setArray(jsonArray);
        QByteArray jsonString = rdoc.toJson(QJsonDocument::Compact);
        response->writeString(jsonString.data());
    });

    httpServer->addPath("/imageload/*", [](Request *request, Response *response) {
        QString index = request->getPathParam("index");
        QString path = allMedias[index.toInt()];
        if (path.isEmpty()) {
            response->write404();
            return;
        }
        QImage image = ThumbnailUtil::getThumnail(path, 200, 200);
        if (image.isNull()) {
            qDebug() << "Failed to load image from:" << path;
            response->write404();
            return;
        }
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "jpg", 100);
        response->writeBytes(byteArray, ContentTypes::contentTypeMap["jpg"]);
    });

    httpServer->addPath("/drawable", [](Request *request, Response *response) {
        QString name = request->getPathParam("name");
        QString filePath = "/web/web/drawable/" + name + ".png";
        QFile file(":" + filePath);
        response->writeFile(file);
    });

    httpServer->addPath("/favicon.ico", [](Request *request, Response *response) {
        QString filePath = "/web/web/images/lanshare.png";
        QFile file(":" + filePath);
        response->writeFile(file);
    });

    // 获取图片
    httpServer->addPath("/images/*", [](Request *request, Response *response) {
        QString path = request->getRequestURL();
        QString filePath = "/web/web";
        filePath += path;
        QFile file(":" + filePath);
        response->writeFile(file);
    });

    // 主页
    httpServer->addPath("/css/*", [](Request *request, Response *response) {
        QString path = request->getRequestURL();
        QString filePath = "/web/web";
        filePath += path;
        QFile file(":" + filePath);
        response->writeFile(file);
    });

    httpServer->addPath("/js/*", [](Request *request, Response *response) {
        QString path = request->getRequestURL();
        QString filePath = "/web/web";
        filePath += path;
        QFile file(":" + filePath);
        response->writeFile(file);
    });

    // 主页
    httpServer->addPath("/", [](Request *request, Response *response) {
        QString filePath = "/web/web";
        filePath += "/lanshare.html";
        QFile file(":" + filePath);
        response->writeFile(file);
    });
}

LHttpServer::~LHttpServer() {
    for (auto websocket: websockets) {
        delete websocket;
    }
    websockets.clear();
}
