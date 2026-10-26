//
// Created by fgsqme on 2021/9/26 0026.
//

#include "TCPServer.h"

#include <QDebug>

/** 构造函数 */
TCPServer::TCPServer(int port) : port(port) {
}

/** 析构函数 */
TCPServer::~TCPServer() {
    this->close();
}

/** 创建并绑定一个监听 Socket */
mFd TCPServer::listenOne(int family, int protocolPort) {
    mFd fd = ::socket(family, SOCK_STREAM, 0);
    if (!fdValid(fd)) {
        qDebug("Failed to create %s socket: %d", family == AF_INET ? "IPv4" : "IPv6", netLastError());
        return netInvalidFd();
    }
    // 允许地址复用，便于重启后立即绑定同一端口
    enableReuseAddr(fd);
    if (family == AF_INET6) {
        enableV6Only(fd, true);
    }
    int ret = -1;
    if (family == AF_INET) {
        sockaddr_in servAddr{};
        servAddr.sin_family = AF_INET;
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网卡
        servAddr.sin_port = htons((unsigned short) protocolPort);
        ret = ::bind(fd, (sockaddr *) &servAddr, sizeof(servAddr));
    } else {
        sockaddr_in6 servAddr{};
        servAddr.sin6_family = AF_INET6;
        servAddr.sin6_addr = in6addr_any; // 监听所有网卡
        servAddr.sin6_port = htons((unsigned short) protocolPort);
        ret = ::bind(fd, (sockaddr *) &servAddr, sizeof(servAddr));
    }
    if (ret == -1) {
        qDebug("bind error on %s: %d", family == AF_INET ? "IPv4" : "IPv6", netLastError());
        netClose(fd);
        return netInvalidFd();
    }
    if (::listen(fd, 5) == -1) {
        qDebug("listen error on %s: %d", family == AF_INET ? "IPv4" : "IPv6", netLastError());
        netClose(fd);
        return netInvalidFd();
    }
    return fd;
}

/** 绑定并监听 IPv4 和 IPv6 端口 */
int TCPServer::bind() {
    if (!netInit()) {
        qDebug("Failed to load Winsock.\n");
        return 0;
    }
    ipv4_fd = listenOne(AF_INET, port);
    ipv6_fd = listenOne(AF_INET6, port);
    // 主机未启用 IPv6 时只保留 IPv4 监听即可，反之亦然
    if (!fdValid(ipv4_fd) && !fdValid(ipv6_fd)) {
        return 0;
    }
    qDebug("init TCPServer success! ");
    return 1;
}

/** 接受客户端连接（返回原始 fd） */
mFd TCPServer::acceptFd() {
    // 分片轮询：close() 后两个 fd 均失效，等待循环立即退出
    while (fdValid(ipv4_fd) || fdValid(ipv6_fd)) {
        // 每轮重新采集，已关闭的 fd 不会再次参与轮询
        const std::vector<mFd> fds = {ipv4_fd, ipv6_fd};
        bool errored = false;
        mFd ready = selectReadable(fds, SELECT_POLL_MS, &errored);
        if (!fdValid(ready)) {
            if (errored) {
                qDebug("accept select error: %d", netLastError());
                return netInvalidFd();
            }
            continue;
        }
        mFd newClient = ::accept(ready, nullptr, nullptr);
        if (!fdValid(newClient)) {
            if (netErrRetryable(netLastError())) {
                continue;
            }
            qDebug("accept error: %d", netLastError());
            return netInvalidFd();
        }
        return newClient;
    }
    return netInvalidFd();
}

/** 接受客户端连接 */
std::unique_ptr<TCPClient> TCPServer::accept() {
    mFd newClient = acceptFd();
    if (!fdValid(newClient)) {
        return nullptr;
    }
    return std::make_unique<TCPClient>(intFromFd(newClient));
}

/** 关闭服务器（幂等，可安全重复调用） */
int TCPServer::close() {
    if (fdValid(ipv4_fd)) {
        netClose(ipv4_fd);
    }
    ipv4_fd = netInvalidFd();
    if (fdValid(ipv6_fd)) {
        netClose(ipv6_fd);
    }
    ipv6_fd = netInvalidFd();
    return 0;
}
