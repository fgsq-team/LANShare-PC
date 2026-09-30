//
// Created by fgsq on 2024/1/18.
//

#ifndef LANSHARE_MEDIAIDPATHDBUTIL_H
#define LANSHARE_MEDIAIDPATHDBUTIL_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QString>
#include <QObject>
#include "MediaIdPath.h"

class MediaIdPathDBUtil  {


public:
    MediaIdPathDBUtil();

    ~MediaIdPathDBUtil();

    void
    addMediaIdPath(qint64 id, const QString &name, const QString &path, const QDateTime &creationTime, bool isReceived);

    MediaIdPath queryMediaIdPath(qint64 id);

    QList<MediaIdPath> queryList();

    void deleteMediaIdPath(qint64 id);

    bool isIdExists(qint64 id);

private:
//    QSqlDatabase database;
    QSqlDatabase database;
    QSqlQuery *query;

    void createTable();

    QString formatDate(const QDateTime &dateTime);

    QDateTime parseDate(const QString &dateString);
};

#endif //LANSHARE_MEDIAIDPATHDBUTIL_H
