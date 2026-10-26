//
// Created by fgsqme on 2021/9/21.
//


#include <QString>
#include <QDebug>
#include "UDPClient.h"
#include "TimeTools.h"


/** 创建指定地址族的 UDP Socket 并开启广播能力 */
static mFd createUdpSocket(int family) {
    if (family != AF_INET && family != AF_INET6) {
        return netInvalidFd();
    }
    mFd fd = ::socket(family, SOCK_DGRAM, 0);
    if (!fdValid(fd)) {
        qDebug("UDPClient create socket error: %d", netLastError());
        return netInvalidFd();
    }
    enableReuseAddr(fd);
    // IPv6 下广播不可用，发送组播地址（如 ff02::1）即可，此处只保证 IPv4 广播
    enableBroadcast(fd);
    if (family == AF_INET6) {
        enableV6Only(fd, true);
    }
    return fd;
}

/** 构造函数（绑定到指定 IP，启用广播） */
UDPClient::UDPClient(const QString &ip) {
    if (!netInit()) {
        qDebug("init wsa fail\n");
        return;
    }
    udp_fd = netInvalidFd();
    CLIENT_ADDR srcAddr{};
    socklen_t srcLen = 0;
    if (!resolveAddr(ip, 0, srcAddr, srcLen)) {
        qDebug() << "UDPClient invalid bind ip:" << ip;
        return;
    }
    family = srcAddr.ss_family;
    udp_fd = createUdpSocket(family);
    if (!fdValid(udp_fd)) {
        return;
    }
    if (::bind(udp_fd, (sockaddr *) &srcAddr, srcLen) == -1) {
        qDebug("UDPClient bind error: %d", netLastError());
        netClose(udp_fd);
        udp_fd = netInvalidFd();
    }
}

/** 构造函数（指定目标 IP 和端口） */
UDPClient::UDPClient(const QString &ip, int port) {
    if (!netInit()) {
        qDebug("init wsa fail\n");
        return;
    }
    udp_fd = netInvalidFd();
    socklen_t dstLen = 0;
    if (!resolveAddr(ip, port, dst, dstLen)) {
        qDebug() << "UDPClient invalid target ip:" << ip;
        return;
    }
    family = dst.ss_family;
    hasDst = true;
    udp_fd = createUdpSocket(family);
}

/** 析构函数 */
UDPClient::~UDPClient() {
    this->close();
}

/** 向指定 IP 和端口发送数据 */
int UDPClient::sendto(const QString &ip, int port, const void *buff, int len, int flag) const {
    CLIENT_ADDR dstAddr{};
    socklen_t dstLen = 0;
    if (!resolveAddr(ip, port, dstAddr, dstLen)) {
        qDebug("UDPClient resolve ip fail: %s", ip.toUtf8().constData());
        return -1;
    }
    return sendto(&dstAddr, buff, len, flag);
}

/** 向指定地址发送数据 */
int UDPClient::sendto(const CLIENT_ADDR *dst_addr, const void *buff, int len, int flag) const {
    if (dst_addr == nullptr || !fdValid(udp_fd)) {
        return -1;
    }
    CLIENT_ADDR dstAddr = *dst_addr;
    // 双栈地址（::ffff:a.b.c.d）还原为 IPv4 地址
    unmapV4(dstAddr);
    if (dstAddr.ss_family != family) {
        // 单播地址族与 Socket 地址族不一致时无法在同一个 Socket 上发送
        qDebug("UDPClient address family mismatch: %d != %d", (int) dstAddr.ss_family, family);
        return -1;
    }
    return (int) ::sendto(udp_fd, static_cast<const char *>(buff), len, flag,
                          (sockaddr *) &dstAddr, addrLength(dstAddr));
}


/** 向构造时指定的地址发送数据 */
int UDPClient::send(const void *buff, int len, int flag) {
    if (!hasDst || !fdValid(udp_fd)) {
        return -1;
    }
    return (int) ::sendto(udp_fd, static_cast<const char *>(buff), len, flag,
                          (sockaddr *) &dst, addrLength(dst));
}

/** 接收数据 */
int UDPClient::recv(void *buff, int len, int flag) const {
    if (!fdValid(udp_fd)) {
        return -1;
    }
    int i = (int) ::recv(udp_fd, static_cast<char *>(buff), len, flag);
    return i;
}

/** 完整接收指定长度数据 */
int UDPClient::recvo(void *buff, size_t len, int flag) const {
    return recvo(buff, 0, len, flag);
}

/** 完整接收指定长度数据（带偏移） */
int UDPClient::recvo(void *buff, int index, size_t len, int flag) const {
    auto *tempBuff = (unsigned char *) buff;
    int totalRecv = 0;
    int off = index;
    size_t size = len;
    while (size > 0) {
        int i = recv(reinterpret_cast<char *>(&tempBuff[off]), (int) size, flag);
        if (i == 0) {
            return i;
        } else if (i <= -1) {
            // 数据接收错误，可能客户端断开连接
            qDebug("error during recvall: %d\n", netLastError());
            return i;
        }
        totalRecv += i;
        off += i;
        size -= i;
    }
    return totalRecv;
}


/** 关闭 Socket */
int UDPClient::close() const {
    if (!fdValid(udp_fd)) {
        return 0;
    }
    return netClose(udp_fd);
}
