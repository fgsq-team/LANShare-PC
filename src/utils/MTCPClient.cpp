//
// Created by fgsqme on 2023/1/31.
//

#include "MTCPClient.h"
#include "LException.h"


MTCPClient::MTCPClient(int fd) : TCPClient(fd) {

}

MTCPClient::~MTCPClient() {
    this->close();
}

bool MTCPClient::IsClose() const {
    return isClose;
}

int MTCPClient::send(const void *buff, int len, int flag) const {
    if (isClose) throw LException("recv is close");
    return TCPClient::send(buff, len, flag);
}

int MTCPClient::sendp(const void *buff, int len, int flag) const {
    return TCPClient::send(buff, len, flag);
}

int MTCPClient::recv(void *buff, int len, int flag) const {
    if (isClose) throw LException("recv is close");
    return TCPClient::recv(buff, len, flag);
}

int MTCPClient::recvo(void *buff, size_t len, int flag) const {
    if (isClose) throw LException("recv is close");
    return TCPClient::recvo(buff, len, flag);
}

int MTCPClient::recvo(void *buff, int index, size_t len, int flag) const {
    if (isClose) throw LException("recv is close");
    return TCPClient::recvo(buff, index, len, flag);
}

int MTCPClient::close()  {
    isClose = true;
    return 1;
}


