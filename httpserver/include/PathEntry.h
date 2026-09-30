//
// Created by fgsq on 2023/12/17.
//

#ifndef LANSHARE_PATHENTRY_H
#define LANSHARE_PATHENTRY_H


#include <QString>
#include <QSharedPointer>
#include "HttpHandler.h"

class PathEntry {
private:
    QString path;
    QString method;
    HttpHandler httpHandler;

public:
    PathEntry(const QString &path, HttpHandler httpHandler);

    PathEntry(const QString &path,const QString &method, HttpHandler httpHandler);

    QString getPath() const;

    void setPath(const QString &path);

    HttpHandler getHttpHandler() const;

    void setHttpHandler(HttpHandler httpHandler);

    const QString &getMethod() const;

    void setMethod(const QString &method);
};

#endif //LANSHARE_PATHENTRY_H
