

#ifndef EP_UDPSERVER_H
#define EP_UDPSERVER_H


#include "SocketBase.h"
#include  <QString>

/**
 * UDP 服务器类
 * 封装 UDP Socket 服务器的创建、发送、接收等操作
 * 同时监听 IPv4 与 IPv6，接收时对两个协议族统一轮询
 * @author fgsq
 * @version 2.0
 */
class UDPServer {
private:
    mFd udp4_fd = -1;              // IPv4 Socket 文件描述符
    mFd udp6_fd = -1;              // IPv6 Socket 文件描述符
    int port;                      // 监听端口

    /**
     * 等待任一 Socket 可读（阻塞直到就绪或全部 Socket 关闭）
     * @return 可读的 Socket，全部关闭或出错返回无效 fd
     */
    mFd waitReadable() const;

    /**
     * 按地址族选择用于发送的 Socket
     * @param dst_addr 目标地址
     * @return 对应的 Socket，不存在时返回无效 fd
     */
    mFd fdForAddress(const CLIENT_ADDR &dst_addr) const;

public:
    /**
     * 构造函数
     * 创建 IPv4 / IPv6 Socket 并绑定到指定端口
     * @param port 监听端口
     */
    UDPServer(int port);

    /** 析构函数 */
    ~UDPServer();

    /**
     * 向指定地址发送数据
     * @param src_addr 目标地址
     * @param buff 数据缓冲区
     * @param len 数据长度
     * @param flag 发送标志
     * @return 实际发送的字节数
     */
    int sendto(const CLIENT_ADDR *src_addr, const void *buff, size_t len, int flag = 0) const;

    /**
     * 向指定 IP 和端口发送数据
     * @param ip 目标 IP
     * @param clientPort 目标端口
     * @param buff 数据缓冲区
     * @param len 数据长度
     * @param flag 发送标志
     * @return 实际发送的字节数
     */
    int sendto(const QString& ip, int clientPort, const void *buff, size_t len, int flag = 0) const;

    /**
     * 接收数据
     * @param buff 接收缓冲区
     * @param len 缓冲区大小
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recv(void *buff, size_t len, int flag = 0) const;

    /**
     * 完整接收指定长度数据
     * @param buff 接收缓冲区
     * @param len 期望接收的字节数
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recvo(void *buff, size_t len, int flag = 0) const;

    /**
     * 接收数据并获取发送方地址
     * @param src_addr 发送方地址（输出）
     * @param buff 接收缓冲区
     * @param len 缓冲区大小
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recv(CLIENT_ADDR *src_addr, void *buff, size_t len, int flag = 0);

    /**
     * 完整接收指定长度数据并获取发送方地址
     * @param src_addr 发送方地址（输出）
     * @param buff 接收缓冲区
     * @param len 期望接收的字节数
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recvo(CLIENT_ADDR *src_addr, void *buff, size_t len, int flag = 0);

    /**
     * 关闭 Socket（幂等，可安全重复调用）
     * @return 关闭结果
     */
    int close();
};


#endif //EP_UDPSERVER_H
