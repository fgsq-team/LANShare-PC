//
// Created by fgsqme on 2021/9/26 0026.
//

#ifndef EP_TCPSERVER_H
#define EP_TCPSERVER_H

#include "SocketBase.h"
#include "TCPClient.h"
#include <memory>

/**
 * TCP 服务器类
 * 封装 TCP Socket 服务器的绑定、监听、接受连接等操作
 * 同时支持 IPv4 和 IPv6 双栈监听
 * @author fgsq
 * @version 1.0
 */
class TCPServer {
private:
    mFd ipv4_fd = -1;   // IPv4 Socket 文件描述符
    mFd ipv6_fd = -1;   // IPv6 Socket 文件描述符
    int port;           // 监听端口

public:
    /**
     * 构造函数
     * @param port 监听端口
     */
    TCPServer(int port);

    /** 析构函数 */
    ~TCPServer();

    /**
     * 绑定并监听 IPv4 和 IPv6 端口
     * @return 成功返回 1，失败返回 -1
     */
    int bind();

    /**
     * 接受客户端连接
     * @return TCP 客户端指针，失败返回 nullptr
     */
    std::unique_ptr<TCPClient> accept();

    /**
     * 接受客户端连接（返回原始 fd）
     * 使用 select 同时监听 IPv4 和 IPv6
     * @return 新连接的 fd，失败返回 -1
     */
    mFd acceptFd();

    /**
     * 关闭服务器（幂等，可安全重复调用）
     * @return 关闭结果
     */
    int close();
};


#endif //EP_TCPSERVER_H
