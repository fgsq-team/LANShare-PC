//
// Created by fgsqme on 2021/9/25 0025.
//


#include "UDPServer.h"


UDPServer::~UDPServer() {
    close();
}


UDPServer::UDPServer(int port) : port(port) {
#if defined(PLATFORM_WINDOWS)
    WORD sockVision = MAKEWORD(2, 2);
    WSADATA wsadata;
    if (WSAStartup(sockVision, &wsadata) != 0) {
        qDebug("init wsa fail\n");
        return;
    }
    //创建套接字
    udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_fd == INVALID_SOCKET) {
        qDebug("create socket fail\n");
        return;
    }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        qDebug("create socket fail\n");
        return;
    }
#endif
    sockaddr_in local_addr{};
    local_addr.sin_family = AF_INET;          // 使用IPV4协议
    local_addr.sin_port = htons(port);
    local_addr.sin_addr.s_addr = INADDR_ANY;  // 绑定本地IP
    int ret = bind(udp_fd, (sockaddr *) &local_addr, sizeof local_addr);
    if (ret < 0) {
        qDebug("bind fail:\n");
        close();
        return;
    } else {
        qDebug("init UDPServer Success!");
    }
}

int UDPServer::sendto(const QString &ip, int clientPort, const void *buff, size_t len, int flag) const {
    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(clientPort);
    clientAddr.sin_addr.s_addr = inet_addr(ip.toUtf8().data());
    return sendto(&clientAddr, buff, len, flag);
}


int UDPServer::sendto(CLIENT_ADDR *src_addr, const void *buff, size_t len, int flag) const {
    return ::sendto(udp_fd, static_cast<const char *>(buff), (int) len, flag, (sockaddr *) src_addr, addr_len);
}

int UDPServer::recv(void *buff, size_t len, int flag) const {
    return ::recv(udp_fd, static_cast<char *>(buff), (int)len, flag);
}

int UDPServer::recvo(void *buff, size_t len, int flag) const {
    auto *tempBuff = (unsigned char *) buff;
    int totalRecv = 0;
    size_t size = len;
    while (size > 0) {
        int i = ::recv(udp_fd, reinterpret_cast<char *>(&tempBuff[totalRecv]), (int)size, flag);
        if (i == 0) {
            return i;
        } else if (i == -1) {
            // 数据接收错误，可能客户端断开连接
            qDebug("error during recvall: %d\n", (int) i);
            return i;
        }
        totalRecv += i;
        size -= i;
    }
    return totalRecv;
}


int UDPServer::recv(CLIENT_ADDR *src_addr, void *buff, size_t len, int flag) {
#if defined(PLATFORM_WINDOWS)
    return ::recvfrom(udp_fd, static_cast<char *>(buff), (int)len, flag, (sockaddr *) src_addr, &addr_len);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    return ::recvfrom(udp_fd, static_cast<char *>(buff), len, flag, (sockaddr *) src_addr,
                      reinterpret_cast<socklen_t *>((int *) &addr_len));
#endif
}

int UDPServer::recvo(CLIENT_ADDR *src_addr, void *buff, size_t len, int flag) {
    auto *tempBuff = (unsigned char *) buff;
    int totalRecv = 0;
    size_t size = len;
    while (size > 0) {

#if defined(PLATFORM_WINDOWS)
        int i = recvfrom(udp_fd, reinterpret_cast<char *>(&tempBuff[totalRecv]), (int)size, flag, (sockaddr *) src_addr,
                         &addr_len);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
        int i = recvfrom(udp_fd, &tempBuff[totalRecv], size, flag, (sockaddr *) src_addr,
                         reinterpret_cast<socklen_t *>((int *) (&addr_len)));
#endif
        if (i == 0) {
            return i;
        } else if (i == -1) {
            // 数据接收错误，可能客户端断开连接
            qDebug("error during recvall: %d\n", (int) i);
            return i;
        }
        totalRecv += i;
        size -= i;
    }
    return totalRecv;
}


int UDPServer::close() const {
#if defined(PLATFORM_WINDOWS)
    return ::closesocket(udp_fd);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    return ::close(udp_fd);
#endif
}



