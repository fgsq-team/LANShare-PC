//
// Created by fgsqme on 2023/12/22.
//

#ifndef LANSHARE_LWEBSOCKETSERVER_H
#define LANSHARE_LWEBSOCKETSERVER_H

#include "TCPClient.h"
#include "Request.h"
#include "Response.h"

class LWebSocketServer {

public:
    LWebSocketServer(Request *request, Response *response);

    bool isClosed() const;

    bool isControlFrame(int frame) const;

    QByteArray readFrame();

    qint64 getFrameLength(char mask);

    void sendString(const QByteArray &msg);

    void sendFrame(const QByteArray &data, int opCode);

    void close();


private:
    TCPClient *tcpClient;
    bool closed = false;
    Request *request;
    Response *response;
};


#endif //LANSHARE_LWEBSOCKETSERVER_H
