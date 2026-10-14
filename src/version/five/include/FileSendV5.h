//
// Created by fgsq on 2026/10/12.
//

#ifndef LANSHARE_FILESENDV5_H
#define LANSHARE_FILESENDV5_H

#include <map>
#include <memory>
#include <vector>

#include "CustomDataStream.h"
#include "FileTransferV5.h"
#include "LFile.h"
#include "ProgressCallback.h"
#include "Type.h"

class TCPClient;

/**
 * V5 文件发送器（支持断点续传）
 * <p>V5 为 V4 的超集：使用独立的 {@code NEW_VERSION_5} 头，仅在目标端 {@code dataVersion>=5}
 * 时启用断点续传。请求 JSON 增加 {@code resume=1} 字段，并为可续传文件/子文件分配
 * 稳定 fileId（基于发送设备 UUID + 路径 + 长度生成 MD5），接收端据此回传已完成偏移量表。</p>
 *
 * @author fgsq
 * @version 5.0
 */
class FileSendV5 {
private:
    ProgressCallback *callback;

public:
    FileSendV5();

    /**
     * 发送文件到指定设备（V5，支持断点续传）
     */
    void send(Device fromDevice, Device toDevice, std::unique_ptr<TCPClient> tcpClient,
              std::vector<LFile *> fileList);

    void handleFileTransfer(FileTransferV5 *fileTransfer, TCPClient *tcpClient) const;

    void sendFolder(FileTransferV5 *fileTransfer, CustomDataStream *dataStream, LFile *fileItem) const;

    void sendFile(FileTransferV5 *fileTransfer, CustomDataStream *dataStream, LFile *fileItem) const;

    mlong sendFileStream(FileTransferV5 *fileTransfer, mlong progressBase, mlong folderSize, LFile *baseFileItem,
                         LFile *fileItem, CustomDataStream *dataStream, mlong startOffset) const;

    mlong sendFileStreamEnc(FileTransferV5 *fileTransfer, mlong progressBase, mlong folderSize, LFile *baseFileItem,
                            LFile *fileItem, CustomDataStream *dataStream, mlong startOffset) const;

    static void sendNewVersionFlag(const Device &fromDevice, CustomDataStream dataStream);

private:
    static std::map<QString, mlong> readResumeMap(CustomDataStream &dataStream);

    static mlong clampOffset(mlong offset, mlong length);
};

#endif //LANSHARE_FILESENDV5_H
