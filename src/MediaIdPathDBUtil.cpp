// mediaidpathdbutil.cpp
#include "MediaIdPathDBUtil.h"
#include "Config.h"
#include <QDebug>
#include <QCoreApplication>

MediaIdPathDBUtil::~MediaIdPathDBUtil() {
//    database.close();
    // 不再需要关闭数据库连接
    delete query;
}

MediaIdPathDBUtil::MediaIdPathDBUtil() {

    /*  if (QSqlDatabase::contains("media_id_path_list")) {
          database = QSqlDatabase::database("media_id_path_list");
      } else {
          database = QSqlDatabase::addDatabase("QSQLITE");
          database.setDatabaseName("media_id_path_list.db");
          database.setUserName("lanshare");
          database.setPassword("uacvbtyaw");
      }
      if (!database.open()) {
          qDebug() << "打开数据库文件失败";
          return;
      }
      query = new QSqlQuery(database);
      createTable();*/

    if (!database.isValid() || !database.isOpen()) {
        database = QSqlDatabase::addDatabase("QSQLITE", "media_id_path_list");
        database.setDatabaseName(config.lanshareWorkDirPath + "/media_id_path_list.db");
        database.setUserName("lanshare");
        database.setPassword("uacvbtyaw");

        if (!database.open()) {
            qDebug() << "打开数据库文件失败";
            return;
        }

        query = new QSqlQuery(database);
        createTable();
    }
}


void
MediaIdPathDBUtil::addMediaIdPath(qint64 id, const QString &name, const QString &path, const QDateTime &creationTime,
                                  bool isReceived) {
    qDebug() << "id:" << id;
    query->prepare("INSERT INTO media_id_path (id, name, path, creation_time, is_received) VALUES (?, ?, ?, ?, ?)");
    query->addBindValue(id);
    query->addBindValue(name);
    query->addBindValue(path);
    query->addBindValue(formatDate(creationTime));
    query->addBindValue(isReceived ? 1 : 0);

    if (!query->exec()) {
        qDebug() << "Error adding MediaIdPath:" << query->lastError().text();
    }
}

MediaIdPath MediaIdPathDBUtil::queryMediaIdPath(qint64 id) {
    query->prepare("SELECT name, path, creation_time, is_received FROM media_id_path WHERE id = ?");
    query->addBindValue(id);

    if (query->exec() && query->first()) {
        QString name = query->value("name").toString();
        QString path = query->value("path").toString();
        QDateTime creationTime = parseDate(query->value("creation_time").toString());
        bool isReceived = query->value("is_received").toBool();

        return MediaIdPath(id, name, path, creationTime, isReceived);
    }

    return MediaIdPath(); // Return an empty MediaIdPath if not found or error
}

QList<MediaIdPath> MediaIdPathDBUtil::queryList() {
    QList<MediaIdPath> list;
    query->prepare("SELECT id, name, path, creation_time, is_received FROM media_id_path");

    if (query->exec()) {
        while (query->next()) {
            qint64 id = query->value("id").toLongLong();
            QString name = query->value("name").toString();
            QString path = query->value("path").toString();
            QDateTime creationTime = parseDate(query->value("creation_time").toString());
            bool isReceived = query->value("is_received").toBool();

            list.append(MediaIdPath(id, name, path, creationTime, isReceived));
        }
    } else {
        qDebug() << "Error querying MediaIdPath list:" << query->lastError().text();
    }

    return list;
}

void MediaIdPathDBUtil::deleteMediaIdPath(qint64 id) {
    query->prepare("DELETE FROM media_id_path WHERE id = ?");
    query->addBindValue(id);

    if (!query->exec()) {
        qDebug() << "Error deleting MediaIdPath:" << query->lastError().text();
    }
}

bool MediaIdPathDBUtil::isIdExists(qint64 id) {
    query->prepare("SELECT id FROM media_id_path WHERE id = ?");
    query->addBindValue(id);
    return query->exec() && query->first();
}

void MediaIdPathDBUtil::createTable() {
    query->exec(
            "CREATE TABLE IF NOT EXISTS media_id_path (id INTEGER PRIMARY KEY, name TEXT, path TEXT, creation_time TEXT, is_received INTEGER)");
}

QString MediaIdPathDBUtil::formatDate(const QDateTime &dateTime) {
    return dateTime.toString("yyyy-MM-dd HH:mm:ss");
}

QDateTime MediaIdPathDBUtil::parseDate(const QString &dateString) {
    return QDateTime::fromString(dateString, "yyyy-MM-dd HH:mm:ss");
}
