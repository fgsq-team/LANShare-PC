//
// Created by fgsqme on 2021/9/26 0026.
//

#include "TCPClient.h"
#include <utility>
#include <QtGlobal>


TCPClient::TCPClient() {
    setConnected(false);
}

TCPClient::TCPClient(int tcp_fd) : tcp_fd(tcp_fd) {
    setConnected(true);
}

TCPClient::TCPClient(QString ip, int port) : ip(std::move(ip)), port(port) {
}

TCPClient::~TCPClient() {
}

int TCPClient::getRemotePort() const {
    sockaddr_storage addr; // 使用 sockaddr_storage 来兼容 IPv4 和 IPv6
    socklen_t len = sizeof(addr);

    if (getpeername(tcp_fd, (struct sockaddr *) &addr, &len) == -1) {
        qDebug("Error getting remote address");
        return -1; // 返回 -1 表示获取失败
    }
    if (addr.ss_family == AF_INET) {
        // IPv4
        sockaddr_in *addr_in = (struct sockaddr_in *) &addr;
        return ntohs(addr_in->sin_port);
    } else if (addr.ss_family == AF_INET6) {
        // IPv6
        sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *) &addr;
        return ntohs(addr_in6->sin6_port);
    } else {
        qDebug("Unsupported address family");
        return -1; // 返回 -1 表示不支持的地址类型
    }
}

QString TCPClient::getRemoteIP() const {
    sockaddr_storage addr; // 使用 sockaddr_storage 来兼容 IPv4 和 IPv6
    socklen_t len = sizeof(addr);

    if (getpeername(tcp_fd, (struct sockaddr *) &addr, &len) == -1) {
        qDebug("Error getting remote address");
        return "";
    }

    char ipStr[INET6_ADDRSTRLEN]; // 用于存放地址字符串
    if (addr.ss_family == AF_INET) {
        // IPv4
        sockaddr_in *addr_in = (struct sockaddr_in *) &addr;
        inet_ntop(AF_INET, &addr_in->sin_addr, ipStr, sizeof(ipStr));
    } else if (addr.ss_family == AF_INET6) {
        // IPv6
        sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *) &addr;
        inet_ntop(AF_INET6, &addr_in6->sin6_addr, ipStr, sizeof(ipStr));
    } else {
        qDebug("Unsupported address family");
        return "";
    }

    return ipStr; // 返回 IP 地址字符串
}

bool TCPClient::connect() {
    // 用 getaddrinfo 自动解析 IPv4/IPv6（AF_UNSPEC），
    // 旧实现写死 AF_INET + inet_addr，遇到 IPv6 地址会连接失败。
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = nullptr;
    if (getaddrinfo(ip.toUtf8().constData(), std::to_string(port).c_str(), &hints, &res) != 0) {
        qDebug("getaddrinfo failed");
        setConnected(false);
        return false;
    }
    bool ok = false;
    for (struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
        tcp_fd = (int) socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (tcp_fd == -1) {
            continue;
        }
        if (::connect(tcp_fd, p->ai_addr, (int) p->ai_addrlen) != -1) {
            ok = true;
            break;
        }
        ::closesocket(tcp_fd);
        tcp_fd = -1;
    }
    freeaddrinfo(res);
    if (ok) {
        qDebug("connection succeed");
        setConnected(true);
    } else {
        qDebug("connection failed");
        setConnected(false);
    }
    return ok;
}

int TCPClient::send(const void *buff, int len, int flag) const {
    return ::send(tcp_fd, static_cast<const char *>(buff), len, flag);
}

int TCPClient::send(char b) const {
    return send(&b, 1);
}

int TCPClient::read() const {
    char r = 0;
    int rel = recv(&r, 1);
    if (rel <= 0) {
        return -1;
    }
    return r;
}

int TCPClient::recv(void *buff, int len, int flag) const {
    return ::recv(tcp_fd, static_cast<char *>(buff), len, flag);
}

int TCPClient::recvo(void *buff, size_t len, int flag) const {
    return recvo(buff, 0, len, flag);
}

int TCPClient::recvo(void *buff, int index, size_t len, int flag) const {
    auto *tempBuff = (unsigned char *) buff;
    int totalRecv = 0;
    int off = index;
    size_t size = len;
    while (size > 0) {
        int i = ::recv(tcp_fd, reinterpret_cast<char *>(&tempBuff[off]), (int) size, flag);
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


int TCPClient::close() {
#if defined(PLATFORM_WINDOWS)
    return ::closesocket(tcp_fd);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    return ::close(tcp_fd);
#endif
}

int TCPClient::getFd() const {
    return tcp_fd;
}


void TCPClient::setFd(int fd) {
    tcp_fd = fd;
}

long TCPClient::skip(mlong l) const {
    char buffer[1024];
    size_t totalBytesSkipped = 0;
    ssize_t bytesRead;
    while (l > 0) {
        bytesRead = recv(buffer, qMin((int) sizeof(buffer), (int) l));
        if (bytesRead == -1) {
            return 0; // 或者可以返回 SIZE_MAX 表示失败
        }
        if (bytesRead == 0) {
            // 连接关闭或出现其他问题
            return (long) totalBytesSkipped;
        }
        // 跳过接收到的字节数
        l -= bytesRead;
        totalBytesSkipped += bytesRead;
    }
    return (long) totalBytesSkipped;
}

bool TCPClient::isConnected() const {
    return connected;
}

void TCPClient::setConnected(bool isConnected) {
    TCPClient::connected = isConnected;
}
