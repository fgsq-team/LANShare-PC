//
// Created by fgsqme on 2021/9/26 0026.
//

#include "TCPServer.h"
#if defined(PLATFORM_WINDOWS)
TCPServer::TCPServer(int port) : port(port) {
}

TCPServer::~TCPServer() {
    this->close();
}

int TCPServer::bind() {

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        qDebug("Failed to load Winsock.\n");
        return -1;
    }

    // Create IPv4 socket
    ipv4_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ipv4_fd == -1) {
        qDebug("Failed to create IPv4 socket.");
        return -1;
    }

    // Allow the socket to be reused
    int opt = 1;
    if (setsockopt(ipv4_fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt)) < 0) {
        qDebug("setsockopt(SO_REUSEADDR) failed.");
        return -1;
    }

    // Bind to IPv4 address
    sockaddr_in servAddr{};
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
    servAddr.sin_port = htons(port);

    if (::bind(ipv4_fd, (sockaddr *) &servAddr, sizeof(servAddr)) == -1) {
        qDebug("bind error on IPv4!");
        return -1;
    }

    // Create IPv6 socket
    ipv6_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (ipv6_fd == -1) {
        qDebug("Failed to create IPv6 socket.");
        return -1;
    }

    // Allow the socket to be reused
    if (setsockopt(ipv6_fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &opt, sizeof(opt)) < 0) {
        qDebug("setsockopt(SO_REUSEADDR) failed.");
        return -1;
    }

    // Bind to IPv6 address
    sockaddr_in6 servAddr6{};
    memset(&servAddr6, 0, sizeof(servAddr6));
    servAddr6.sin6_family = AF_INET6;
    servAddr6.sin6_addr = in6addr_any; // Listen on all interfaces
    servAddr6.sin6_port = htons(port);

    if (::bind(ipv6_fd, (sockaddr *) &servAddr6, sizeof(servAddr6)) == -1) {
        qDebug("bind error on IPv6!");
        return -1;
    }

    // Set both sockets to listen
    if (::listen(ipv4_fd, 5) == -1) {
        qDebug("listen error on IPv4!");
        return -1;
    }

    if (::listen(ipv6_fd, 5) == -1) {
        qDebug("listen error on IPv6!");
        return -1;
    }

    qDebug("init TCPServer success! ");
    return 1;
}

mFd TCPServer::acceptFd() {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(ipv4_fd, &readfds);
    FD_SET(ipv6_fd, &readfds);

    // Wait for an incoming connection on either socket
    int max_fd = std::max(ipv4_fd, ipv6_fd);
    if (select(max_fd + 1, &readfds, nullptr, nullptr, nullptr) < 0) {
        qDebug("select error!");
        return -1;
    }

    if (FD_ISSET(ipv4_fd, &readfds)) {
        // Accept on IPv4 socket
        mFd newClient = ::accept(ipv4_fd, nullptr, nullptr);
        if (newClient == -1) {
            qDebug("accept error on IPv4!");
            return -1;
        }
        return newClient;
    } else if (FD_ISSET(ipv6_fd, &readfds)) {
        // Accept on IPv6 socket
        mFd newClient = ::accept(ipv6_fd, nullptr, nullptr);
        if (newClient == -1) {
            qDebug("accept error on IPv6!");
            return -1;
        }
        return newClient;
    }

    return -1; // Shouldn't reach here
}

std::unique_ptr<TCPClient> TCPServer::accept() {
    mFd newClient = acceptFd();
    if (newClient == -1) {
        return nullptr;
    }
    return std::make_unique<TCPClient>(newClient);
}

int TCPServer::close() const {
    ::closesocket(ipv4_fd);
    ::closesocket(ipv6_fd);
    return 0;
}
#else

TCPServer::~TCPServer()
{
    this->close();
}
TCPServer::TCPServer(int port) : port(port) {

#if defined(PLATFORM_WINDOWS)
    // windows需要申请网络权限
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        qDebug("Failed to load Winsock.\n");
        return;
    }
#endif
    ipv4_fd = socket(PF_INET, SOCK_STREAM, 0);
    sockaddr_in servAddr{};
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
    if (::bind(ipv4_fd, (sockaddr *) &servAddr, sizeof(servAddr)) == -1) {
        qDebug("bind error");
        return;
    }
    if (::listen(ipv4_fd, 5) == -1) {
        qDebug("listen error");
        return;
    }
    qDebug("init TCPServer success");
}

int TCPServer::bind()
{
    return 1;
}
int TCPServer::acceptFd()  {
    int newClient = (int) ::accept(ipv4_fd, nullptr, nullptr);
    if (-1 == newClient) {
        qDebug("accept error");
        return -1;
    } else {
        //        qDebug("new client connect");
    }
    return newClient;
}

std::unique_ptr<TCPClient> TCPServer::accept()  {
    int newClient = (int)::accept(ipv4_fd, nullptr, nullptr);
    if (-1 == newClient) {
        qDebug("accept error");
        return nullptr;
    } else {
        // 创建新的TCPClient对象，并返回其智能指针
        return std::make_unique<TCPClient>(newClient);
    }
}


int TCPServer::close() const {
    return ::close(ipv4_fd);
}


#endif
