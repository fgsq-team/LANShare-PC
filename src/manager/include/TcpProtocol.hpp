//
// Created by fgsq on 2026/10/6.
//

#ifndef LANSHARE_TCPPROTOCOL_H
#define LANSHARE_TCPPROTOCOL_H

#include "Device.h"
#include "TCPClient.h"
#include "DataDec.h"
#include "DataEnc.h"
#include <memory>
#include <vector>

class LANShare;
class LFile;

/**
 * TCP 协议处理类
 * 负责处理 TCP 连接的建立与消息处理
 * @author fgsq
 * @version 1.0
 */
class TcpProtocol {
private:
    LANShare *lanshare;

    void handleFileShareRequest(Device &device, DataDec &dataDec, TCPClient *tcpClient, mbyte *buffer) const;

public:
    /**
     * 构造函数
     * @param lanshare LANShare 主服务指针
     */
    explicit TcpProtocol(LANShare *lanshare);

    /**
     * 创建 TCP 连接
     * @param ip 目标 IP
     * @param port 目标端口
     * @return TCP 客户端指针，失败返回 nullptr
     */
    static std::unique_ptr<TCPClient> makeSocket(QString ip, int port);

    /**
     * 构造 TCP 设备数据包
     * @param device 设备信息
     * @param dataEnc 数据编码器
     */
    static void makeDataEnc(const Device &device, DataEnc *dataEnc);

    /**
     * 处理 TCP 连接
     * @param tcpClient TCP 客户端指针
     */
    void handleTcp(std::unique_ptr<TCPClient> tcpClient);

    /**
     * 创建 TCP 服务器
     * 循环接受 TCP 连接并分发到线程池处理
     */
    void createTcpServer();
};

#endif //LANSHARE_TCPPROTOCOL_H
