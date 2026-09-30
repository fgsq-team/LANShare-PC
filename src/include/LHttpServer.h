//
// Created by fgsqme on 2024/5/7.
//

#ifndef LANSHARE_LHTTPSERVER_H
#define LANSHARE_LHTTPSERVER_H

#include <memory>
#include "HttpServer.h"

class LANShare;

class LHttpServer {
private:
    LANShare *lanShare;
public:
    std::unique_ptr<HttpServer> httpServer;
public:
    LHttpServer(LANShare *lanShare);

    static void sendDeviceList();

    static void sendWebSocketMessage(
            const QString &message, const QString &toDevName, const QString &filePath,
            int messageType, int devType, const QString &fileSize,
            bool isLeft, bool isClip, bool isFile = true
    );

    ~LHttpServer();
};


#endif //LANSHARE_LHTTPSERVER_H
