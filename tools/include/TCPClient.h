//
// Created by fgsqme on 2021/9/26 0026.
//

#ifndef EP_TCPCLIENT_H
#define EP_TCPCLIENT_H

#include "SocketBase.h"
#include <QString>

class TCPClient {
private:
    int tcp_fd = -1;
    QString ip;
    int port = -1;
    bool connected{};
public:
    TCPClient(QString ip, int port);

    TCPClient(int tcp_fd);

    TCPClient();

    ~TCPClient();

    int getRemotePort() const;

    bool connect();

    bool isConnected() const;

    void setConnected(bool connected);

    int send(const void *buff, int len, int flag = 0) const;

    int send(char b) const;

    int recv(void *buff, int len, int flag = 0) const;

    int read() const;

    long skip(mlong l) const;

    int recvo(void *buff, size_t len, int flag = 0) const;

    int recvo(void *buff, int index, size_t len, int flag = 0) const;

    int close() ;

    int getFd() const;

    void setFd(int fd);

    QString getRemoteIP() const;
};


#endif //EP_TCPCLIENT_H
