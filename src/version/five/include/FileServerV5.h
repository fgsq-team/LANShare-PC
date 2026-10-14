//
// Created by fgsq on 2026/10/12.
//

#ifndef LANSHARE_FILESERVERV5_H
#define LANSHARE_FILESERVERV5_H

#include <memory>
#include <vector>

#include <QJsonObject>
#include <QString>

#include "CustomDataStream.h"
#include "FileTransferV5.h"
#include "IOUtils.h"
#include "LFile.h"
#include "ProgressCallback.h"
#include "Type.h"

class TCPClient;
class Device;

/**
 * V5 文件接收器（支持断点续传）
 * <p>V5 拥有独立的 {@code handleVersion} 入口，由 {@code TcpProtocol} 在检测到 {@code NEW_VERSION_5}
 * 头时直接分发进入，不再经过 V4 {@code FileServer}。文件传输相关方法全部自包含于本类；
 * 非文件类命令（消息/媒体同步）则调用 V4/V5 共用的 {@code SyncFeatureHandler}。</p>
 * <p>流程：同意接收后先根据已存在的 {@code .part} 分段计算每个 fileId 的已完成偏移并回传给发送端，
 * 发送端据此跳过已完成字节；接收端将后续字节追加写入分段文件，完成后再落地为最终文件。</p>
 *
 * @author fgsq
 * @version 5.0
 */
class FileServerV5 {
private:
    ProgressCallback *callback;

public:
    FileServerV5();

    /**
     * V5 独立版本入口：读取命令字并分发（文件/消息/媒体同步）。
     * @param device     发送方设备
     * @param tcpClient  Socket 连接所有权
     */
    void handleVersion(const Device &device, std::unique_ptr<TCPClient> tcpClient);

    /**
     * 处理 V5 断点续传文件传输请求（从流中读取 encData 与请求 JSON）。
     * @param fromDevice   发送方设备
     * @param stream       自定义数据流（关联同一 socket）
     * @param tcpClient    Socket 连接所有权
     */
    void handleFileTransfer(const Device &fromDevice, CustomDataStream *stream,
                            std::unique_ptr<TCPClient> tcpClient);

    /**
     * 开始接收文件：同意/拒绝、回传续传偏移量表、逐文件/文件夹接收。
     */
    void startReceiveFile(FileTransfer *fileTransfer, std::vector<LFile *> files, CustomDataStream *stream,
                          bool encData, bool isAgree);

private:
    void handleFile(CustomDataStream *stream, FileTransferV5 *fileTransfer, LFile *fileItem, bool encData) const;

    void receiveDirect(CustomDataStream *stream, FileTransferV5 *fileTransfer, LFile *fileItem, bool encData) const;

    void handleFolder(CustomDataStream *stream, FileTransferV5 *fileTransfer, LFile *fileItem, bool encData) const;

    void handleFolderDirect(CustomDataStream *stream, FileTransferV5 *fileTransfer, LFile *fileItem,
                            bool encData) const;

    mlong recvStream(CustomDataStream *stream, FileTransfer *fileTransfer, IOUtils *out, mlong finalSize,
                     LFile *baseFileItem, const LFile *fileItem, mlong startOffset, mlong progressBase,
                     bool encData) const;

    static QString buildResumeMapJson(const std::vector<LFile *> &files);

    static IOUtils *openPart(const QString &part, mlong startOffset);

    static QString saveDir();

    static QString baseTarget(const QString &name);

    static QString landingPathOf(const QString &name);
};

#endif //LANSHARE_FILESERVERV5_H
