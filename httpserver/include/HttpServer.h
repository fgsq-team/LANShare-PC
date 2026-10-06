//
// Created by fgsq on 2023/12/17.
//

#ifndef LANSHARE_HTTPSERVER_H
#define LANSHARE_HTTPSERVER_H


#include <QSharedPointer>
#include <memory>
#include "PathEntry.h"

class HttpServer : public std::enable_shared_from_this<HttpServer> {

public :
    HttpServer();

    explicit HttpServer(int port);

    void addPath(const QString &path, HttpHandler handler);

    void addPath(const QString &path, const QString &method, HttpHandler handler);

    void startServer();

    void newClient(TCPClient *tcpClient, const QString &method);

    void setRequestFilter(RequestFilter requestFilter);

    static bool pathMatches(const QString &registeredPathPattern, const QString &requestPath);

private:
    RequestFilter requestFilter = nullptr;
    QList<PathEntry> handlerMap;
    int port{};

private:
    void sortPath();

};


#endif //LANSHARE_HTTPSERVER_H
