

#ifndef EP_UDPSERVER_H
#define EP_UDPSERVER_H


#include "SocketBase.h"
#include  <QString>

#define CLIENT_ADDR sockaddr_in

class UDPServer {
private:
    mFd udp_fd;
    int port;
    int addr_len = sizeof(sockaddr_in);
public:
    UDPServer(int port);

    ~UDPServer();

    int sendto(sockaddr_in *src_addr, const void *buff, size_t len, int flag = 0) const;

    int sendto(const QString& ip, int clientPort, const void *buff, size_t len, int flag = 0) const;

    int recv(void *buff, size_t len, int flag = 0) const;

    int recvo(void *buff, size_t len, int flag = 0) const;

    int recv(sockaddr_in *src_addr, void *buff, size_t len, int flag = 0);

    int recvo(sockaddr_in *src_addr, void *buff, size_t len, int flag = 0);


    int close() const;
};


#endif //EP_UDPSERVER_H
