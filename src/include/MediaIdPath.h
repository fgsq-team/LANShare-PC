//
// Created by fgsq on 2024/1/18.
//

#ifndef LANSHARE_MEDIAIDPATH_H
#define LANSHARE_MEDIAIDPATH_H


// mediaidpath.h
#ifndef MEDIAIDPATH_H
#define MEDIAIDPATH_H

#include <QObject>
#include <QDateTime>

class MediaIdPath
{
public:
    MediaIdPath();
    MediaIdPath(qint64 id, const QString &name, const QString &path, const QDateTime &creationTime, bool isReceived);

    qint64 getId() const;
    void setId(qint64 id);

    QString getName() const;
    void setName(const QString &name);

    QString getPath() const;
    void setPath(const QString &path);

    QDateTime getCreationTime() const;
    void setCreationTime(const QDateTime &creationTime);

    bool isReceived() const;
    void setReceived(bool isReceived);

    QString toString() const;

private:
    qint64 id;
    QString name;
    QString path;
    QDateTime creationTime;
    bool received;
};

#endif // MEDIAIDPATH_H

#endif //LANSHARE_MEDIAIDPATH_H
