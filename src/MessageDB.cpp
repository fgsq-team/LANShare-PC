//
// Created by fgsq on 2023/10/9.
//
#include <QSqlQuery>
#include <QDebug>
#include <QUuid>
#include <QSqlError>
#include <QtMath>
#include "MessageDB.h"
#include "MessageFile.h"
#include "TimeTools.h"
#include "Config.h"
#include "StringLockManager.h"

#define DB_VERSION 2

MessageDB::MessageDB() {
    if (QSqlDatabase::contains("message")) {
        database = QSqlDatabase::database("message");
    } else {
        database = QSqlDatabase::addDatabase("QSQLITE", "message");
        database.setDatabaseName(config.lanshareWorkDirPath + "/message_v" + QString::number(DB_VERSION) + ".db");
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
        "isdel INTEGER)"
    )) {
        qDebug() << "创建表失败";
    } else {
        qDebug() << "创建表成功";
    }
}

MessageDB::~MessageDB() {
    if (database.isOpen()) {
        database.close();
    }
    delete sql_query;
}

void MessageDB::updateMessage(Message *message) {
    if (!database.isOpen()) {
        return;
    }
    QString sql = "UPDATE message SET message=:message, "
            "messageType=:messageType, "
            "deviceName=:deviceName, "
            "dataVersion=:dataVersion, "
            "toUser=:toUser, "
            "isLeft=:isLeft, "
            "isfile=:isfile, "
            "devMode=:devMode, "
            "filePath=:filePath, "
            "completed=:completed, "
            "recviced=:recviced, "
            "fileSize=:fileSize, "
            "fileSizeStr=:fileSizeStr, "
            "isVideo=:isVideo, "
            "isdel=:isdel "
            "WHERE id=:id";
    sql_query->prepare(sql);
    sql_query->bindValue(":message", message->getMessage());
    sql_query->bindValue(":deviceName", message->getDeviceName());
    sql_query->bindValue(":dataVersion", message->getDataVersion());
    sql_query->bindValue(":isLeft", message->isLeft());
    sql_query->bindValue(":isfile", true);
    sql_query->bindValue(":devMode", message->getDevMode());
    sql_query->bindValue(":isdel", false);
    sql_query->bindValue(":isVideo", false);
    sql_query->bindValue(":videoTime", "");
    sql_query->bindValue(":toUser", "");
    sql_query->bindValue(":id", message->getUuid());
    if (typeid(*message) == typeid(MessageFile)) {
        auto *messageFile = (MessageFile *) message;
        sql_query->bindValue(":messageType", MESSAGE_FILE_TYPE);
        sql_query->bindValue(":filePath", messageFile->getFilePath());
        sql_query->bindValue(":completed", messageFile->isCompleted());
        sql_query->bindValue(":recviced", messageFile->isRecviced());
        sql_query->bindValue(":fileSize", QVariant::fromValue(messageFile->getFileSize()));
        sql_query->bindValue(":fileSizeStr", messageFile->getFileSizeStr());
    } else {
        sql_query->bindValue(":messageType", MESSAGE_TYPE);
        sql_query->bindValue(":filePath", "");
        sql_query->bindValue(":completed", false);
        sql_query->bindValue(":recviced", false);
        sql_query->bindValue(":fileSize", 0);
        sql_query->bindValue(":fileSizeStr", "");
    }
    if (!sql_query->exec()) {
        qDebug() << "更新数据失败: " << sql_query->lastError();
    }
}


void MessageDB::addMessage(Message *message) {
    if (!database.isOpen()) {
        return;
    }
    std::mutex &addMessageLock = StringLockManager::getStringLock("addMessage");
    std::lock_guard<std::mutex> lock(addMessageLock); // 自动锁定 mtx
    QString uuid = message->getUuid();
    //    if (uuid == nullptr || uuid.length() <= 0) {
    //        uuid = Utils::getUUID();
    //    }
    // 使用占位符进行插入操作
    sql_query->prepare("INSERT INTO message VALUES("
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
        ":isdel"
        ")"
    );
    sql_query->bindValue(":dataVersion", message->getDataVersion());
    sql_query->bindValue(":createTime", TimeTools::getFormatTime().c_str());
    sql_query->bindValue(":timeStamp", TimeTools::getCurrentTime());
    sql_query->bindValue(":isVideo", false);
    sql_query->bindValue(":videoTime", "");
    sql_query->bindValue(":toUser", "");
    if (typeid(*message) == typeid(MessageFile)) {
        auto *messageFile = (MessageFile *) message;
        sql_query->bindValue(":uuid", uuid);
        sql_query->bindValue(":messageType", MESSAGE_FILE_TYPE);
        sql_query->bindValue(":message", message->getMessage());
        sql_query->bindValue(":devName", message->getDeviceName());
        sql_query->bindValue(":isLeft", message->isLeft());
        sql_query->bindValue(":isFile", true);
        sql_query->bindValue(":devMode", message->getDevMode());
        sql_query->bindValue(":filePath", messageFile->getFilePath());
        sql_query->bindValue(":completed", messageFile->isCompleted());
        sql_query->bindValue(":recviced", messageFile->isRecviced());
        sql_query->bindValue(":fileSize", QVariant::fromValue(messageFile->getFileSize()));
        sql_query->bindValue(":fileSizeStr", messageFile->getFileSizeStr());
        sql_query->bindValue(":fileSizeStr", messageFile->getFileSizeStr());
        sql_query->bindValue(":isdel", false);
    } else {
        sql_query->bindValue(":uuid", uuid);
        sql_query->bindValue(":messageType", MESSAGE_TYPE);
        sql_query->bindValue(":message", mUtils::encMessage(message->getMessage(), KEY));
        sql_query->bindValue(":devName", message->getDeviceName());
        sql_query->bindValue(":isLeft", message->isLeft());
        sql_query->bindValue(":isFile", false);
        sql_query->bindValue(":devMode", message->getDevMode());
        sql_query->bindValue(":filePath", "");
        sql_query->bindValue(":completed", false);
        sql_query->bindValue(":recviced", false);
        sql_query->bindValue(":fileSize", false);
        sql_query->bindValue(":fileSizeStr", "");
        sql_query->bindValue(":isdel", false);
    }
    if (!sql_query->exec()) {
        qDebug() << "Error inserting into table:" << sql_query->lastError();
    }
}

std::list<Message *> MessageDB::queryList(int pageSize, int pageCount, QWidget *parent) {
    std::list<Message *> messageList;
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
            long fileSize = sql_query->value(index++).toLongLong();
            QString fileSizeStr = sql_query->value(index++).toString();
            QString createTime = sql_query->value(index++).toString();
            mlong timeStamp = sql_query->value(index++).toLongLong();
            long isdel = sql_query->value(index++).toBool();
            if (messageType == MESSAGE_FILE_TYPE) {
                auto *msg = new MessageFile(messageEnc, deviceName, devMode, left, isfile, parent);
                msg->setFileSize(fileSize);
                msg->setFileSizeStr(fileSizeStr);
                msg->setCompleted(completed);
                msg->setRecviced(true);
                msg->setFilePath(filePath);
                msg->setUuid(id);
                messageList.push_back(msg);
            } else if (messageType == MESSAGE_TYPE) {
                QString message = mUtils::decMessage(messageEnc, KEY);
                auto *msg = new Message(message, deviceName, devMode, left, parent);
                msg->setUuid(id);
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
