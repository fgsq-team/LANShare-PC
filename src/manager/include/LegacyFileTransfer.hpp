//
// Created by fgsq on 2026/10/6.
//

#ifndef LANSHARE_LEGACYFILETRANSFER_H
#define LANSHARE_LEGACYFILETRANSFER_H

#include "Device.h"
#include "LFile.h"
#include "TCPClient.h"
#include "DataEnc.h"
#include "DataDec.h"
#include <memory>
#include <vector>

class LANShare;
class CustomDataStream;
class FileTransfer;

/**
 * 旧版文件传输处理类
 * 负责处理旧版协议的文件发送与接收
 * @author fgsq
 * @version 1.0
 */
class LegacyFileTransfer {
private:
    LANShare *lanshare;

    static mlong baseSend(LFile *file, LFile *f, mlong size, mlong totalFileSize, DataEnc &dataEnc);

    static mlong baseSendEnc(LFile *file, LFile *f, mlong size, mlong totalFileSize, DataEnc &dataEnc);

    static mlong baseRecv(const LFile *mfile, IOUtils &fileIO, DataDec &dataDec, mlong mTotalRecv);

    static mlong baseRecvDec(const LFile *mfile, IOUtils &fileIO, DataDec &dataDec, mlong mTotalRecv);

public:
    /**
     * 构造函数
     * @param lanshare LANShare 主服务指针
     */
    explicit LegacyFileTransfer(LANShare *lanshare);

    /**
     * 发送文件到指定设备
     * @param device 目标设备
     * @param selectFiles 待发送文件列表
     * @param count 文件数量
     */
    void sendFile(const Device &device, std::vector<LFile *> selectFiles, int count);

    /**
     * 开始处理接收文件
     * @param accept 是否接受接收
     * @param device 发送方设备信息
     * @param needEncData 是否需要解密数据
     * @param files 待接收文件列表
     * @param tcpClient TCP 客户端指针
     */
    void startHandleRecvFile(bool accept, const Device &device, bool needEncData,
                             const std::vector<LFile *> &files,
                             const std::unique_ptr<TCPClient> &tcpClient);

    /**
     * 新版文件接收处理
     * @param fromDevice 发送方设备信息
     * @param fileTransfer 文件传输对象
     * @param files 待接收文件列表
     * @param stream 自定义数据流
     * @param encData 是否加密数据
     * @param isAgree 是否同意接收
     */
    void startNewVersionHandleRecvFile(Device fromDevice, FileTransfer *fileTransfer,
                                       std::vector<LFile *> files, CustomDataStream *stream,
                                       bool encData, bool isAgree);
};

#endif //LANSHARE_LEGACYFILETRANSFER_H
