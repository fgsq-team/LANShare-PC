//
// Created by fgsqme on 2021/9/25 0025.
//


#include "UDPServer.h"

#include <QDebug>


/** 析构函数 */
UDPServer::~UDPServer() {
    close();
}


/** 构造函数，创建 IPv4 / IPv6 Socket 并绑定端口 */
UDPServer::UDPServer(int port) : port(port) {
    if (!netInit()) {
        qDebug("init wsa fail\n");
        return;
    }
    udp4_fd = netInvalidFd();
    udp6_fd = netInvalidFd();

    // IPv4：绑定所有网卡的任意地址
    mFd fd4 = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fdValid(fd4)) {
        enableReuseAddr(fd4);
        sockaddr_in v4{};
        v4.sin_family = AF_INET;
        v4.sin_port = htons((unsigned short) port);
        v4.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(fd4, (sockaddr *) &v4, sizeof(v4)) == -1) {
            qDebug("udp ipv4 bind fail: %d", netLastError());
            netClose(fd4);
        } else {
            enableBroadcast(fd4);
            udp4_fd = fd4;
        }
    } else {
        qDebug("create ipv4 udp socket fail: %d", netLastError());
    }

    // IPv6：显式设为仅处理 IPv6，避免与上面的 IPv4 Socket 抢占同一端口
    mFd fd6 = ::socket(AF_INET6, SOCK_DGRAM, 0);
    if (fdValid(fd6)) {
        enableReuseAddr(fd6);
        enableV6Only(fd6, true);
        sockaddr_in6 v6{};
        v6.sin6_family = AF_INET6;
        v6.sin6_port = htons((unsigned short) port);
        v6.sin6_addr = in6addr_any;
        if (::bind(fd6, (sockaddr *) &v6, sizeof(v6)) == -1) {
            // 主机未启用 IPv6 时仅保留 IPv4 监听，不影响正常通信
            qDebug("udp ipv6 bind fail: %d", netLastError());
            netClose(fd6);
        } else {
            enableBroadcast(fd6);
            udp6_fd = fd6;
        }
    } else {
        qDebug("create ipv6 udp socket fail: %d", netLastError());
    }

    if (!fdValid(udp4_fd) && !fdValid(udp6_fd)) {
        qDebug("UDPServer init fail!\n");
        return;
    }
    qDebug("init UDPServer Success!");
}

/** 等待任一 Socket 可读 */
mFd UDPServer::waitReadable() const {
    // 分片轮询：任一 Socket 被关闭后循环立即结束，避免线程卡在无限等待上
    while (fdValid(udp4_fd) || fdValid(udp6_fd)) {
        // 每轮重新采集，已关闭的 fd 不会再次参与轮询
        const std::vector<mFd> fds = {udp4_fd, udp6_fd};
        bool errored = false;
        mFd ready = selectReadable(fds, SELECT_POLL_MS, &errored);
        if (fdValid(ready)) {
            return ready;
        }
        if (errored) {
            // select 出错说明句柄已不可用（通常由 close 引发），直接退出等待
            break;
        }
    }
    return netInvalidFd();
}

/** 按地址族选择用于发送的 Socket */
mFd UDPServer::fdForAddress(const CLIENT_ADDR &dst_addr) const {
    CLIENT_ADDR tmp = dst_addr;
    // 双栈地址（::ffff:a.b.c.d）还原后走 IPv4 Socket
    unmapV4(tmp);
    if (tmp.ss_family == AF_INET) {
        return fdValid(udp4_fd) ? udp4_fd : netInvalidFd();
    }
    if (tmp.ss_family == AF_INET6) {
        return fdValid(udp6_fd) ? udp6_fd : netInvalidFd();
    }
    return netInvalidFd();
}

/** 向指定 IP 和端口发送数据 */
int UDPServer::sendto(const QString &ip, int clientPort, const void *buff, size_t len, int flag) const {
    CLIENT_ADDR dst{};
    socklen_t dstLen = 0;
    if (!resolveAddr(ip, clientPort, dst, dstLen)) {
        qDebug("resolve sendto ip fail: %s", ip.toUtf8().constData());
        return -1;
    }
    return sendto(&dst, buff, len, flag);
}


/** 向指定地址发送数据 */
int UDPServer::sendto(const CLIENT_ADDR *src_addr, const void *buff, size_t len, int flag) const {
    if (src_addr == nullptr) {
        return -1;
    }
    CLIENT_ADDR dst = *src_addr;
    unmapV4(dst);
    mFd fd = fdForAddress(dst);
    if (!fdValid(fd)) {
        return -1;
    }
    return (int) ::sendto(fd, static_cast<const char *>(buff), (int) len, flag,
                          (sockaddr *) &dst, addrLength(dst));
}

/** 接收数据 */
int UDPServer::recv(void *buff, size_t len, int flag) const {
    mFd ready = waitReadable();
    if (!fdValid(ready)) {
        return -1;
    }
    int ret = (int) ::recv(ready, static_cast<char *>(buff), (int) len, flag);
    if (ret < 0) {
        qDebug("udp recv error: %d", netLastError());
    }
    return ret;
}

/** 完整接收指定长度数据 */
int UDPServer::recvo(void *buff, size_t len, int flag) const {
    auto *tempBuff = (unsigned char *) buff;
    int totalRecv = 0;
    size_t size = len;
    while (size > 0) {
        int i = recv(tempBuff + totalRecv, size, flag);
        if (i <= 0) {
            return i;
        }
        totalRecv += i;
        size -= (size_t) i;
    }
    return totalRecv;
}


/** 接收数据并获取发送方地址 */
int UDPServer::recv(CLIENT_ADDR *src_addr, void *buff, size_t len, int flag) {
    if (src_addr == nullptr) {
        return -1;
    }
    mFd ready = waitReadable();
    if (!fdValid(ready)) {
        return -1;
    }
    socklen_t addrLen = sizeof(CLIENT_ADDR);
    memset(src_addr, 0, sizeof(CLIENT_ADDR));
    int ret = (int) ::recvfrom(ready, static_cast<char *>(buff), (int) len, flag,
                               (sockaddr *) src_addr, &addrLen);
    if (ret > 0 && isV4Mapped(*src_addr)) {
        // 双栈 Socket 收到的 IPv4 发送方以映射地址上报，还原为普通 IPv4 地址
        unmapV4(*src_addr);
    }
    return ret;
}

/** 完整接收指定长度数据并获取发送方地址 */
int UDPServer::recvo(CLIENT_ADDR *src_addr, void *buff, size_t len, int flag) {
    auto *tempBuff = (unsigned char *) buff;
    int totalRecv = 0;
    size_t size = len;
    while (size > 0) {
        int i = recv(src_addr, tempBuff + totalRecv, size, flag);
        if (i <= 0) {
            return i;
        }
        totalRecv += i;
        size -= (size_t) i;
    }
    return totalRecv;
}


/** 关闭 Socket（幂等，可安全重复调用） */
int UDPServer::close() {
    if (fdValid(udp4_fd)) {
        netClose(udp4_fd);
        udp4_fd = netInvalidFd();
    }
    if (fdValid(udp6_fd)) {
        netClose(udp6_fd);
        udp6_fd = netInvalidFd();
    }
    return 0;
}
