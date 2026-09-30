//
// Created by fgsqme on 2021/9/21.
//


#include <QString>
#include <QDebug>
#include "UDPClient.h"
#include "TimeTools.h"


UDPClient::UDPClient(const QString &ip) {
#if defined(PLATFORM_WINDOWS)
    WORD sockVision = MAKEWORD(2, 2);
    WSADATA wsadata;
    if (WSAStartup(sockVision, &wsadata) != 0) {
        qDebug("init wsa fail\n");
        return;
    }
#endif
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    /* 本地端口和地址 */
    sockaddr_in src_addr{};
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = 0;
    src_addr.sin_addr.s_addr = inet_addr(ip.toUtf8().data());
    int ret = bind(udp_fd, (sockaddr *) &src_addr, sizeof(sockaddr));
    if (-1 == ret) {
        qDebug() << "UDPClient bind error:" << ret;
        return;
    }
    int i = 1;
    int len = sizeof(i);
    ret = setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, (const char *) (&i), len);
    if (-1 == ret) {
        qDebug() << "UDPClient setsockopt error:" << ret;
        return;
    }
}

UDPClient::~UDPClient() {
    this->close();
}

UDPClient::UDPClient(const QString &ip, int port) {
#if defined(PLATFORM_WINDOWS)
    WORD sockVision = MAKEWORD(2, 2);
    WSADATA wsadata;
    if (WSAStartup(sockVision, &wsadata) != 0) {
        qDebug("init wsa fail\n");
        return;
    }
#endif

    int i = 1;
    int len = sizeof(i);
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&i), len);


    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip.toUtf8().data());
}

int UDPClient::sendto(const QString &ip, int port, const void *buff, int len, int flag) const {
    /* 目的端口和地址 */
    sockaddr_in dst_addr{};
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_port = htons(port);
    dst_addr.sin_addr.s_addr = inet_addr(ip.toUtf8().data());    // 广播地址
    return ::sendto(udp_fd, (char *) buff, len, flag, (sockaddr *) &dst_addr, sizeof(dst_addr));
}


int UDPClient::send(const void *buff, int len, int flag) {
    return ::sendto(udp_fd, static_cast<const char *>(buff), len, flag, (sockaddr *) &addr, sizeof(addr));
}

int UDPClient::recv(void *buff, int len, int flag) const {
    int i = ::recv(udp_fd, static_cast<char *>(buff), len, flag);
    return i;
}

int UDPClient::recvo(void *buff, size_t len, int flag) const {
    return recvo(buff, 0, len, flag);
}

int UDPClient::recvo(void *buff, int index, size_t len, int flag) const {
    auto *tempBuff = (unsigned char *) buff;
    int totalRecv = 0;
    int off = index;
    size_t size = len;
    while (size > 0) {
        int i = ::recv(udp_fd, reinterpret_cast<char *>(&tempBuff[off]), (int) size, flag);
        if (i == 0) {
            return i;
        } else if (i == -1) {
            // 数据接收错误，可能客户端断开连接
            qDebug("error during recvall: %d\n", (int) i);
            return i;
        }
        totalRecv += i;
        off += i;
        size -= i;
    }
    return totalRecv;
}


int UDPClient::close() const {
#if defined(PLATFORM_WINDOWS)
    return ::closesocket(udp_fd);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    return ::close(udp_fd);
#endif
}


