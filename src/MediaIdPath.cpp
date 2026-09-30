// mediaidpath.cpp
#include "MediaIdPath.h"

MediaIdPath::MediaIdPath() {
}

MediaIdPath::MediaIdPath(qint64 id, const QString &name, const QString &path, const QDateTime &creationTime,
                         bool isReceived)
        : id(id), name(name), path(path), creationTime(creationTime), received(isReceived) {
}

qint64 MediaIdPath::getId() const {
    return id;
}

void MediaIdPath::setId(qint64 id) {
    this->id = id;
}

QString MediaIdPath::getName() const {
    return name;
}

void MediaIdPath::setName(const QString &name) {
    this->name = name;
}

QString MediaIdPath::getPath() const {
    return path;
}

void MediaIdPath::setPath(const QString &path) {
    this->path = path;
}

QDateTime MediaIdPath::getCreationTime() const {
    return creationTime;
}

void MediaIdPath::setCreationTime(const QDateTime &creationTime) {
    this->creationTime = creationTime;
}

bool MediaIdPath::isReceived() const {
    return received;
}

void MediaIdPath::setReceived(bool isReceived) {
    this->received = isReceived;
}

QString MediaIdPath::toString() const {
    return QString("MediaIdPath{id=%1, name='%2', path='%3', creationTime=%4, isReceived=%5}")
            .arg(id)
            .arg(name)
            .arg(path)
            .arg(creationTime.toString())
            .arg(received);
}
