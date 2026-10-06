//
// Created by fgsqme on 2021/9/26 0026.
//

#include "TCPClient.h"
#include <utility>
#include <QtGlobal>


/** 默认构造函数 */
TCPClient::TCPClient() {
    setConnected(false);
}

/** 构造函数（已连接 fd） */
TCPClient::TCPClient(int tcp_fd) : tcp_fd(tcp_fd) {
    setConnected(true);
}

/** 构造函数（指定 IP 和端口） */
TCPClient::TCPClient(QString ip, int port) : ip(std::move(ip)), port(port) {

}

/** 析构函数 */
TCPClient::~TCPClient() {
}

/** 获取对端 IP 地址 */
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

/** 发起 TCP 连接 */
bool TCPClient::connect() {
    tcp_fd = (int) socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servAddr{};
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(ip.toStdString().c_str());
    servAddr.sin_port = htons(port);
    if (::connect(tcp_fd, (struct sockaddr *) &servAddr, sizeof(servAddr)) == -1) {
        qDebug("connection failed");
        setConnected(false);
        return false;
    } else {
        qDebug("connection succeed");
        setConnected(true);
        return true;
    }
}

/** 发送数据 */
int TCPClient::send(const void *buff, int len, int flag) const {
    return ::send(tcp_fd, static_cast<const char *>(buff), len, flag);
}

/** 发送单字节 */
int TCPClient::send(char b) const {
    return send(&b, 1);
}

/** 读取单字节 */
int TCPClient::read() const {
    char r = 0;
    int rel = recv(&r, 1);
    if (rel <= 0) {
        return -1;
    }
    return r;
}

/** 接收数据 */
int TCPClient::recv(void *buff, int len, int flag) const {
    return ::recv(tcp_fd, static_cast<char *>(buff), len, flag);
}

/** 完整接收指定长度数据 */
int TCPClient::recvo(void *buff, size_t len, int flag) const {
    return recvo(buff, 0, len, flag);
}

/** 完整接收指定长度数据（带偏移） */
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


/** 关闭连接 */
int TCPClient::close()  {
#if defined(PLATFORM_WINDOWS)
    return ::closesocket(tcp_fd);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    return ::close(tcp_fd);
#endif
}

/** 获取 Socket fd */
int TCPClient::getFd() const {
    return tcp_fd;
}


/** 设置 Socket fd */
void TCPClient::setFd(int fd) {
    tcp_fd = fd;
}

/** 跳过指定字节数 */
mlong TCPClient::skip(mlong l) const {
    char buffer[1024];
    size_t totalBytesSkipped = 0;
    ssize_t bytesRead;
    while (l > 0) {
        bytesRead = recv(buffer, qMin((int) sizeof(buffer), (int) l));
        if (bytesRead == -1) {
            return 0;  // 或者可以返回 SIZE_MAX 表示失败
        }
        if (bytesRead == 0) {
            // 连接关闭或出现其他问题
            return (mlong) totalBytesSkipped;
        }
        // 跳过接收到的字节数
        l -= bytesRead;
        totalBytesSkipped += bytesRead;
    }
    return (mlong) totalBytesSkipped;
}

/** 获取连接状态 */
bool TCPClient::isConnected() const {
    return connected;
}

/** 设置连接状态 */
void TCPClient::setConnected(bool isConnected) {
    TCPClient::connected = isConnected;
}


