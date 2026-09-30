//
// Created by fgsqme on 2021/9/26 0026.
//

#ifndef EP_TCPSERVER_H
#define EP_TCPSERVER_H

#include "SocketBase.h"
#include "TCPClient.h"
#include <memory>

class TCPServer {
private:
    mFd ipv4_fd = -1;
    mFd ipv6_fd = -1;
    int port;

public:
    TCPServer(int port);

    ~TCPServer();

    int bind();

    std::unique_ptr<TCPClient> accept();

    mFd acceptFd();

    int close() const;
};


#endif //EP_TCPSERVER_H
