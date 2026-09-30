#ifndef EP_UDPCLIENT_H
#define EP_UDPCLIENT_H

#include <string>
#include "SocketBase.h"

class UDPClient {
private:
    mFd udp_fd = -1;
    sockaddr_in addr{};

public:

    UDPClient(const QString& ip);

    ~UDPClient();

    UDPClient(const QString& ip, int port);

    int sendto(const QString& ip, int port, const void *buff, int len, int flag = 0) const;

    int send(const void *buff, int len, int flag = 0);

    int recv(void *buff, int len, int flag = 0) const;

    int recvo(void *buff, size_t len, int flag = 0) const;

    int close() const;

    int recvo(void *buff, int index, size_t len, int flag) const;
};


#endif //EP_UDPCLIENT_H
