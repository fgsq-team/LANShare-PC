//
// Created by fgsq on 2023/10/9.
//
#include <QSqlQuery>
#include <QDebug>
#include <QUuid>
#include <QSqlError>
#include <QtMath>
#include "MessageDB.h"
#include "TimeTools.h"
#include "Config.hpp"
#include "StringLockManager.h"

#define DB_VERSION 2

// messageType 列取值（与历史数据库保持一致）
#define MESSAGE_TYPE 1       // 文本
#define MESSAGE_FILE_TYPE 2  // 文件/文件夹
#define MESSAGE_IMAGE_TYPE 3 // 图片（缩略图展示，字段与文件消息同构）

MessageDB::MessageDB() {
    if (QSqlDatabase::contains("message")) {
        database = QSqlDatabase::database("message");
    } else {
        database = QSqlDatabase::addDatabase("QSQLITE", "message");
        database.setDatabaseName(Config::instance().lanshareWorkDirPath + "/message_v" + QString::number(DB_VERSION) + ".db");
        database.setUserName("lanshare");
        database.setPassword("uacvbtyaw");
    }
    if (!database.open()) {
        qDebug() << "打开数据库文件失败";
        return;
    }
    sql_query = new QSqlQuery(database);
    //创建表格
    if (!sql_query->exec("create table if not exists message("
        "id VARCHAR(32) primary key,"
        "messageType INTEGER,"
        "message text,"
        "deviceName text,"
        "dataVersion INTEGER,"
        "toUser text,"
        "isLeft INTEGER,"
        "isfile INTEGER,"
        "devMode INTEGER,"
        "filepath text,"
        "completed INTEGER,"
        "recviced INTEGER,"
        "isVideo INTEGER,"
        "videoTime VARCHAR(19),"
        "fileSize INTEGER,"
        "fileSizeStr text,"
        "createTime varchar,"
        "timeStamp INTEGER,"
        "isdel INTEGER,"
        "progress INTEGER)"
    )) {
        qDebug() << "创建表失败";
    } else {
        qDebug() << "创建表成功";
    }
    // 旧库升级：早期 message_v2 没有 progress 列，缺失时补一列（存量行取默认 0）
    bool hasProgress = false;
    if (sql_query->exec("PRAGMA table_info(message)")) {
        while (sql_query->next()) {
            if (sql_query->value(1).toString().compare("progress", Qt::CaseInsensitive) == 0) {
                hasProgress = true;
                break;
            }
        }
    }
    if (!hasProgress) {
        if (!sql_query->exec("ALTER TABLE message ADD COLUMN progress INTEGER DEFAULT 0")) {
            qDebug() << "添加 progress 列失败: " << sql_query->lastError();
        }
    }
}

MessageDB::~MessageDB() {
    if (database.isOpen()) {
        database.close();
    }
    delete sql_query;
}

void MessageDB::updateMessage(const ChatMessage &message) {
    if (!database.isOpen()) {
        return;
    }
    QString sql = "UPDATE message SET message=:message, "
            "messageType=:messageType, "
            "deviceName=:deviceName, "
            "dataVersion=:dataVersion, "
            "isLeft=:isLeft, "
            "isfile=:isfile, "
            "devMode=:devMode, "
            "filePath=:filePath, "
            "completed=:completed, "
            "recviced=:recviced, "
            "fileSize=:fileSize, "
            "fileSizeStr=:fileSizeStr, "
            "progress=:progress, "
            "isdel=:isdel "
            "WHERE id=:id";
    sql_query->prepare(sql);

    const ContentElement *file = message.firstOf(ElementType::File);
    const ContentElement *img = message.firstOf(ElementType::Image);
    const ContentElement *media = file != nullptr ? file : img;

    sql_query->bindValue(":deviceName", message.deviceName);
    sql_query->bindValue(":dataVersion", message.dataVersion);
    sql_query->bindValue(":isLeft", message.direction == ChatDirection::Left);
    sql_query->bindValue(":devMode", message.devMode);
    sql_query->bindValue(":isdel", false);
    sql_query->bindValue(":id", message.uuid);

    if (media != nullptr) {
        sql_query->bindValue(":messageType", file != nullptr ? MESSAGE_FILE_TYPE : MESSAGE_IMAGE_TYPE);
        sql_query->bindValue(":message", media->name);
        sql_query->bindValue(":isfile", !media->isDirectory);
        sql_query->bindValue(":filePath", media->filePath);
        sql_query->bindValue(":completed", media->completed);
        sql_query->bindValue(":recviced", media->received);
        sql_query->bindValue(":fileSize", QVariant::fromValue(media->size));
        sql_query->bindValue(":fileSizeStr", media->sizeStr);
        sql_query->bindValue(":progress", media->progress);
    } else {
        sql_query->bindValue(":messageType", MESSAGE_TYPE);
        sql_query->bindValue(":message", message.displayText());
        sql_query->bindValue(":isfile", false);
        sql_query->bindValue(":filePath", "");
        sql_query->bindValue(":completed", false);
        sql_query->bindValue(":recviced", false);
        sql_query->bindValue(":fileSize", 0);
        sql_query->bindValue(":fileSizeStr", "");
        sql_query->bindValue(":progress", 0);
    }
    if (!sql_query->exec()) {
        qDebug() << "更新数据失败: " << sql_query->lastError();
    }
}

void MessageDB::addMessage(const ChatMessage &message) {
    if (!database.isOpen()) {
        return;
    }
    std::mutex &addMessageLock = StringLockManager::getStringLock("addMessage");
    std::lock_guard<std::mutex> lock(addMessageLock); // 自动锁定 mtx
    const QString &uuid = message.uuid;

    // 使用占位符进行插入操作；V5 链路的消息 id 取自稳定 fileId，重发/被删除后
    // 再次传输可能命中同一主键，用 REPLACE 语义避免插入失败丢消息
    sql_query->prepare("INSERT OR REPLACE INTO message VALUES("
        ":uuid,"
        ":messageType,"
        ":message,"
        ":devName,"
        ":dataVersion,"
        ":toUser,"
        ":isLeft,"
        ":isFile,"
        ":devMode,"
        ":filePath,"
        ":completed,"
        ":recviced,"
        ":isVideo,"
        ":videoTime,"
        ":fileSize,"
        ":fileSizeStr,"
        ":createTime,"
        ":timeStamp,"
        ":isdel,"
        ":progress"
        ")"
    );
    sql_query->bindValue(":uuid", uuid);
    const ContentElement *fileEl = message.firstOf(ElementType::File);
    const ContentElement *imgEl = message.firstOf(ElementType::Image);
    const ContentElement *media = fileEl != nullptr ? fileEl : imgEl;
    sql_query->bindValue(":devName", message.deviceName);
    sql_query->bindValue(":dataVersion", message.dataVersion);
    sql_query->bindValue(":createTime", TimeTools::getFormatTime().c_str());
    sql_query->bindValue(":timeStamp", TimeTools::getCurrentTime());
    sql_query->bindValue(":isVideo", false);
    sql_query->bindValue(":videoTime", "");
    sql_query->bindValue(":toUser", "");
    sql_query->bindValue(":isLeft", message.direction == ChatDirection::Left);
    sql_query->bindValue(":devMode", message.devMode);
    sql_query->bindValue(":isdel", false);

    if (media != nullptr) {
        sql_query->bindValue(":messageType", fileEl != nullptr ? MESSAGE_FILE_TYPE : MESSAGE_IMAGE_TYPE);
        // 文件/图片消息展示名以明文存储（与历史行为一致）
        sql_query->bindValue(":message", media->name);
        sql_query->bindValue(":isFile", !media->isDirectory);
        sql_query->bindValue(":filePath", media->filePath);
        sql_query->bindValue(":completed", media->completed);
        sql_query->bindValue(":recviced", media->received);
        sql_query->bindValue(":fileSize", QVariant::fromValue(media->size));
        sql_query->bindValue(":fileSizeStr", media->sizeStr);
        sql_query->bindValue(":progress", media->progress);
    } else {
        sql_query->bindValue(":messageType", MESSAGE_TYPE);
        // 文本消息正文加密存储
        sql_query->bindValue(":message", mUtils::encMessage(message.displayText(), KEY));
        sql_query->bindValue(":isFile", false);
        sql_query->bindValue(":filePath", "");
        sql_query->bindValue(":completed", false);
        sql_query->bindValue(":recviced", false);
        sql_query->bindValue(":fileSize", 0);
        sql_query->bindValue(":fileSizeStr", "");
        sql_query->bindValue(":progress", 0);
    }
    if (!sql_query->exec()) {
        qDebug() << "Error inserting into table:" << sql_query->lastError();
    }
}

std::list<ChatMessage> MessageDB::queryList(int pageSize, int pageCount) {
    std::list<ChatMessage> messageList;
    if (!database.isOpen()) {
        return messageList;
    }
    // 获取总记录数
    if (!sql_query->exec("SELECT COUNT(1) FROM message where isdel = false")) {
        qDebug() << "Error executing query:" << sql_query->lastError().text();
        return messageList;
    }
    float totalCount = 0;
    if (sql_query->next()) {
        totalCount = (float) sql_query->value(0).toInt();
    }
    // 计算总页数
    int totalPages = qCeil(totalCount / (float) pageSize);
    // 计算偏移量
    int offset = (totalPages - pageCount - 1) * pageSize;
    if (offset < 0) {
        return messageList;
    }
    sql_query->prepare("select * from message where isdel = false limit :pageSize offset :pageCount");
    sql_query->bindValue(":pageSize", pageSize);
    sql_query->bindValue(":pageCount", offset);
    if (!sql_query->exec()) {
        qDebug() << "执行错误";
    } else {
        while (sql_query->next()) {
            int index = 0;
            QString id = sql_query->value(index++).toString();
            int messageType = sql_query->value(index++).toInt();
            QString messageEnc = sql_query->value(index++).toString();
            QString deviceName = sql_query->value(index++).toString();
            int dataVersion = sql_query->value(index++).toInt();
            QString toUser = sql_query->value(index++).toString();
            bool left = sql_query->value(index++).toBool();
            bool isfile = sql_query->value(index++).toBool();
            int devMode = sql_query->value(index++).toInt();
            QString filePath = sql_query->value(index++).toString();
            bool completed = sql_query->value(index++).toBool();
            bool recviced = sql_query->value(index++).toBool();
            bool isVideo = sql_query->value(index++).toBool();
            QString videoTime = sql_query->value(index++).toString();
            mlong fileSize = sql_query->value(index++).toLongLong();
            QString fileSizeStr = sql_query->value(index++).toString();
            QString createTime = sql_query->value(index++).toString();
            mlong timeStamp = sql_query->value(index++).toLongLong();
            bool isdel = sql_query->value(index++).toBool();
            int savedProgress = sql_query->value(index++).toInt();
            Q_UNUSED(toUser);
            Q_UNUSED(isVideo);
            Q_UNUSED(videoTime);
            Q_UNUSED(isdel);

            ChatDirection dir = left ? ChatDirection::Left : ChatDirection::Right;
            if (messageType == MESSAGE_FILE_TYPE || messageType == MESSAGE_IMAGE_TYPE) {
                const bool image = messageType == MESSAGE_IMAGE_TYPE;
                ChatMessage msg = image
                                      ? ChatMessage::fromImage(QString(), messageEnc, id, deviceName,
                                                               devMode, dir, fileSize)
                                      : ChatMessage::fromFile(messageEnc, id, deviceName, devMode, dir,
                                                              fileSize, !isfile);
                msg.uuid = id;
                msg.dataVersion = dataVersion;
                msg.createTime = createTime;
                msg.timeStamp = timeStamp;
                ContentElement *el = msg.firstOf(image ? ElementType::Image : ElementType::File);
                if (el != nullptr) {
                    el->filePath = filePath;
                    el->sizeStr = fileSizeStr;
                    el->completed = completed;
                    el->received = recviced || completed;
                    el->progress = completed ? 100 : qBound(0, savedProgress, 99);
                }
                messageList.push_back(msg);
            } else if (messageType == MESSAGE_TYPE) {
                QString text = mUtils::decMessage(messageEnc, KEY);
                ChatMessage msg = ChatMessage::fromText(text, deviceName, devMode, dir);
                msg.uuid = id;
                msg.dataVersion = dataVersion;
                msg.createTime = createTime;
                msg.timeStamp = timeStamp;
                messageList.push_back(msg);
            }
        }
    }
    return messageList;
}

void MessageDB::deleteMessage(const QString &uuid) {
    if (!database.isOpen()) {
        return;
    }
    QString sql = "update message set isdel = -1 where id = \"" + uuid + "\" ";
    qDebug() << sql.toUtf8().data();
    if (!sql_query->exec(sql)) {
        qDebug() << "删除数据失败";
    }
}

void MessageDB::deleteAllMessage() {
    if (!database.isOpen()) {
        return;
    }
    QString sql = "update message set isdel = -1";
    qDebug() << sql.toUtf8().data();
    if (!sql_query->exec(sql)) {
        qDebug() << "删除数据失败";
    }
}
