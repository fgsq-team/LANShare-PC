//
// Created by fgsqme on 2023/1/31.
//

#ifndef LANSHARE_WIN_MIOUTIL_H
#define LANSHARE_WIN_MIOUTIL_H

#include "IOUtils.h"
#include "TCPClient.h"

/**
 * 可管理 TCP 客户端类
 * 继承 TCPClient，增加关闭状态检查，发送/接收前自动检查连接状态
 * @author fgsq
 * @version 1.0
 */
class MTCPClient : public TCPClient {
private:
    bool isClose = false;  // 关闭标志
public:
    /**
     * 构造函数
     * @param fd Socket 文件描述符
     */
    MTCPClient(int fd);

    /** 析构函数 */
    ~MTCPClient();

    /**
     * 判断是否已关闭
     * @return 已关闭返回 true
     */
    bool IsClose() const;

    /**
     * 发送数据（已关闭时抛异常）
     * @param buff 数据缓冲区
     * @param len 数据长度
     * @param flag 发送标志
     * @return 实际发送的字节数
     */
    int send(const void *buff, int len, int flag = 0) const;

    /**
     * 发送数据（不检查关闭状态）
     * @param buff 数据缓冲区
     * @param len 数据长度
     * @param flag 发送标志
     * @return 实际发送的字节数
     */
    int sendp(const void *buff, int len, int flag = 0) const;

    /**
     * 接收数据（已关闭时抛异常）
     * @param buff 接收缓冲区
     * @param len 缓冲区大小
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recv(void *buff, int len, int flag = 0) const;

    /**
     * 完整接收指定长度数据（已关闭时抛异常）
     * @param buff 接收缓冲区
     * @param len 期望接收的字节数
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recvo(void *buff, size_t len, int flag = 0) const;

    /**
     * 完整接收指定长度数据（带偏移，已关闭时抛异常）
     * @param buff 接收缓冲区
     * @param index 缓冲区偏移
     * @param len 期望接收的字节数
     * @param flag 接收标志
     * @return 实际接收的字节数
     */
    int recvo(void *buff, int index, size_t len, int flag = 0) const;

    /**
     * 关闭连接（仅设置关闭标志）
     * @return 1
     */
    int close() ;

};


#endif //LANSHARE_WIN_MIOUTIL_H
