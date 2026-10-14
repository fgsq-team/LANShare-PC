//
// Created by fgsq on 2026/10/12.
//

#ifndef LANSHARE_SYNCFEATUREHANDLER_H
#define LANSHARE_SYNCFEATUREHANDLER_H

class CustomDataStream;
class Device;

/**
 * 同步类功能处理器（V4 / V5 共用）
 * <p>承载与文件传输无关、跨版本完全一致的两类命令处理：文本消息接收与媒体同步。
 * 由 {@code FileServer::handleVersion1} 与 {@code FileServerV5::handleVersion} 共同调用，
 * 避免同一逻辑在各版本包中重复实现。</p>
 *
 * @author fgsq
 * @version 5.0
 */
class SyncFeatureHandler {
public:
    /**
     * 接收文本消息：解密后广播到 UI，必要时写入剪贴板，随后关闭连接。
     * @param device 发送方设备
     * @param stream 自定义数据流（关联同一 socket）
     */
    static void handleMessage(const Device &device, CustomDataStream *stream);

    /**
     * 媒体同步（暂时不支持，直接关闭连接）。
     * @param device 发送方设备
     * @param stream 自定义数据流（关联同一 socket）
     */
    static void handleMediaSync(const Device &device, CustomDataStream *stream);
};

#endif //LANSHARE_SYNCFEATUREHANDLER_H
