//
// Created by fgsq on 2026/10/6.
//

#ifndef LANSHARE_UDPPROTOCOL_H
#define LANSHARE_UDPPROTOCOL_H

#include "Device.h"

class LANShare;

/**
 * UDP 协议处理类
 * 负责处理 UDP 消息的接收与广播
 * @author fgsq
 * @version 1.0
 */
class UdpProtocol {
private:
    LANShare *lanshare;

public:
    /**
     * 构造函数
     * @param lanshare LANShare 主服务指针
     */
    explicit UdpProtocol(LANShare *lanshare);

    /**
     * 处理 UDP 消息接收
     * 循环监听并处理各类 UDP 消息
     */
    void handleUdp();

    /**
     * 广播消息到指定设备或所有在线设备
     * @param toDevice 目标设备，为空则广播给所有设备
     * @param message 消息内容
     * @param isClip 是否写入剪切板
     * @param shareWS 是否同步到 WebSocket
     */
    void broadcastMessage(Device *toDevice, const QString &message, bool isClip, bool shareWS);
};

#endif //LANSHARE_UDPPROTOCOL_H
