//
// Created by fgsqme on 2021/9/26 0026.
//

#include "TCPClient.h"
#include <utility>
#include <QtGlobal>
#include <QDebug>


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
    CLIENT_ADDR addr{};
    socklen_t len = sizeof(addr);

    if (::getpeername(fd(), (sockaddr *) &addr, &len) == -1) {
        qDebug("Error getting remote address: %d", netLastError());
        return "";
    }

    // 双栈 Socket 上接入的 IPv4 客户端会以 ::ffff:x.x.x.x 形式上报，还原为 IPv4
    unmapV4(addr);
    const QString host = addrHost(addr);
    if (host.isEmpty()) {
        qDebug("Unsupported address family");
    }
    return host;
}

/** 发起 TCP 连接 */
bool TCPClient::connect() {
    if (!netInit()) {
        qDebug("init wsa fail\n");
        setConnected(false);
        return false;
    }
    // 重连前先释放旧句柄
    if (isFdValid()) {
        netClose(fd());
        tcp_fd = -1;
    }
    /* 目标地址支持 IPv4、IPv6（可带 %区域索引）与主机名 */
    CLIENT_ADDR servAddr{};
    socklen_t addrLen = 0;
    if (!resolveAddr(ip, port, servAddr, addrLen)) {
        qDebug("resolve address failed: %s", ip.toUtf8().constData());
        setConnected(false);
        return false;
    }
    mFd sock = ::socket(servAddr.ss_family, SOCK_STREAM, 0);
    if (!fdValid(sock)) {
        qDebug("create socket failed: %d", netLastError());
        setConnected(false);
        return false;
    }
    if (::connect(sock, (sockaddr *) &servAddr, addrLen) == -1) {
        qDebug("connection failed: %s", ip.toUtf8().constData());
        netClose(sock);
        setConnected(false);
        return false;
    }
    qDebug("connection succeed");
    tcp_fd = intFromFd(sock);
    setConnected(true);
    return true;
}

/** 发送数据 */
int TCPClient::send(const void *buff, int len, int flag) const {
    if (!isFdValid()) {
        return -1;
    }
    return (int) ::send(fd(), static_cast<const char *>(buff), len, flag);
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
    if (!isFdValid()) {
        return -1;
    }
    return (int) ::recv(fd(), static_cast<char *>(buff), len, flag);
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
        int i = this->recv(reinterpret_cast<char *>(&tempBuff[off]), (int) size, flag);
        if (i == 0) {
            return i;
        } else if (i == -1) {
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


/** 关闭连接 */
int TCPClient::close()  {
    if (!isFdValid()) {
        return 0;
    }
    int ret = netClose(fd());
    tcp_fd = -1;
    setConnected(false);
    return ret;
}

/** 获取 Socket fd */
int TCPClient::getFd() const {
    return tcp_fd;
}

/** Socket 句柄是否有效 */
bool TCPClient::isFdValid() const {
    return fdValid(fd());
}

/** 以平台句柄形式返回当前 fd */
mFd TCPClient::fd() const {
    return fdFromInt(tcp_fd);
}


/** 设置 Socket fd */
void TCPClient::setFd(int fd) {
    tcp_fd = fd;
}

/** 跳过指定字节数 */
mlong TCPClient::skip(mlong l) const {
    char buffer[1024];
    size_t totalBytesSkipped = 0;
    // 使用 int 而非 ssize_t，避开 MSVC 等平台未定义 ssize_t 的兼容问题
    int bytesRead;
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


