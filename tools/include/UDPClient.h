#ifndef EP_UDPCLIENT_H
#define EP_UDPCLIENT_H

#include <string>
#include "SocketBase.h"

/**
 * UDP 客户端类
 * 封装 UDP Socket 的创建、发送、接收等操作
 * 支持广播模式
 * @author fgsq
 * @version 1.0
 */
class UDPClient {
private:
    mFd udp_fd = -1;       // Socket 文件描述符
    sockaddr_in addr{};    // 目标地址

public:
    /**
     * 构造函数（绑定到指定 IP，启用广播）
     * @param ip 绑定 IP
     */
    UDPClient(const QString& ip);

    /** 析构函数 */
    ~UDPClient();

    /**
     * 构造函数（指定目标 IP 和端口）
     * @param ip 目标 IP
     * @param port 目标端口
     */
    UDPClient(const QString& ip, int port);

    /**
     * 向指定 IP 和端口发送数据
     * @param ip 目标 IP
     * @param port 目标端口
     * @param buff 数据缓冲区
     * @param len 数据长度
     * @param flag 发送标志
     * @return 实际发送的字节数
     */
    int sendto(const QString& ip, int port, const void *buff, int len, int flag = 0) const;

    /**
     * 向构造时指定的地址发送数据
     * @param buff 数据缓冲区
     * @param len 数据长度
     * @param flag 发送标志
     * @return 实际发送的字节数
     */
    int send(const void *buff, int len, int flag = 0);

    /**
     * 接收数据
     * @param buff 接收缓冲区
     * @param len 缓冲区大小
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recv(void *buff, int len, int flag = 0) const;

    /**
     * 完整接收指定长度数据
     * @param buff 接收缓冲区
     * @param len 期望接收的字节数
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recvo(void *buff, size_t len, int flag = 0) const;

    /**
     * 关闭 Socket
     * @return 关闭结果
     */
    int close() const;

    /**
     * 完整接收指定长度数据（带偏移）
     * @param buff 接收缓冲区
     * @param index 缓冲区偏移
     * @param len 期望接收的字节数
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recvo(void *buff, int index, size_t len, int flag) const;
};


#endif //EP_UDPCLIENT_H
