//
// Created by fgsqme on 2021/9/26 0026.
//

#ifndef EP_TCPCLIENT_H
#define EP_TCPCLIENT_H

#include "SocketBase.h"
#include <QString>

/**
 * TCP 客户端类
 * 封装 TCP Socket 的连接、发送、接收等操作
 * 连接目标同时支持 IPv4、IPv6 与主机名
 * @author fgsq
 * @version 2.0
 */
class TCPClient {
private:
    int tcp_fd = -1;       // Socket 文件描述符（Windows 下为 SOCKET 句柄的整型形式）
    QString ip;            // 目标 IP
    int port = -1;         // 目标端口
    bool connected{};      // 连接状态

    /** Socket 句柄是否有效 */
    bool isFdValid() const;

    /** 以平台句柄（mFd）形式返回当前 fd */
    mFd fd() const;
public:
    /**
     * 构造函数（指定 IP 和端口，用于主动连接）
     * @param ip 目标 IP（IPv4 / IPv6 / 主机名）
     * @param port 目标端口
     */
    TCPClient(QString ip, int port);

    /**
     * 构造函数（指定已连接的 fd，用于服务器接受连接）
     * @param tcp_fd 已连接的 Socket fd
     */
    TCPClient(int tcp_fd);

    /** 默认构造函数 */
    TCPClient();

    /** 析构函数 */
    ~TCPClient();

    /**
     * 发起 TCP 连接
     * @return 连接成功返回 true
     */
    bool connect();

    /**
     * 获取连接状态
     * @return 是否已连接
     */
    bool isConnected() const;

    /**
     * 设置连接状态
     * @param connected 连接状态
     */
    void setConnected(bool connected);

    /**
     * 发送数据
     * @param buff 数据缓冲区
     * @param len 数据长度
     * @param flag 发送标志
     * @return 实际发送的字节数
     */
    int send(const void *buff, int len, int flag = 0) const;

    /**
     * 发送单字节
     * @param b 待发送字节
     * @return 实际发送的字节数
     */
    int send(char b) const;

    /**
     * 接收数据
     * @param buff 接收缓冲区
     * @param len 缓冲区大小
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recv(void *buff, int len, int flag = 0) const;

    /**
     * 读取单字节
     * @return 读取的字节值，失败返回 -1
     */
    int read() const;

    /**
     * 跳过指定字节数
     * @param l 要跳过的字节数
     * @return 实际跳过的字节数
     */
    mlong skip(mlong l) const;

    /**
     * 完整接收指定长度数据（阻塞直到收完）
     * @param buff 接收缓冲区
     * @param len 期望接收的字节数
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recvo(void *buff, size_t len, int flag = 0) const;

    /**
     * 完整接收指定长度数据（带偏移）
     * @param buff 接收缓冲区
     * @param index 缓冲区偏移
     * @param len 期望接收的字节数
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recvo(void *buff, int index, size_t len, int flag = 0) const;

    /**
     * 关闭连接
     * @return 关闭结果
     */
    int close() ;

    /**
     * 获取 Socket 文件描述符
     * @return fd
     */
    int getFd() const;

    /**
     * 设置 Socket 文件描述符
     * @param fd Socket fd
     */
    void setFd(int fd);

    /**
     * 获取对端 IP 地址
     * IPv4 映射地址（::ffff:x.x.x.x）会还原为 IPv4 字符串
     * @return IP 地址字符串
     */
    QString getRemoteIP() const;
};


#endif //EP_TCPCLIENT_H
