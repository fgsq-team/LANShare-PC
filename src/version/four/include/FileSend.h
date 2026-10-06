//
// Created by 16508 on 2025/8/30.
//

#ifndef LANSHARE_FILESEND_H
#define LANSHARE_FILESEND_H
#include "ProgressCallback.h"


class FileSend {
private:
    ProgressCallback *callback;
public:
    FileSend();
    void send(Device fromDevice, Device toDevice, std::unique_ptr<TCPClient> tcpClient, std::vector<LFile *> fileList);
    void handleFileTransfer(FileTransfer *fileTransfer, TCPClient *tcpClient) const;
    void sendFolder(FileTransfer *fileTransfer, CustomDataStream *dataStream, LFile *fileItem) const;
    void sendFile(FileTransfer *fileTransfer, CustomDataStream *dataStream, LFile *fileItem) const;
    mlong sendFileStream(FileTransfer *fileTransfer, mlong total, mlong folderSize, LFile *baseFileItem, LFile *fileItem,
                        CustomDataStream *dataStream) const;
    mlong sendFileStreamEnc(FileTransfer *fileTransfer, mlong total, mlong folderSize, LFile *baseFileItem, LFile *fileItem,
                      CustomDataStream *dataStream) const;
    static void sendNewVersionFlag(const Device &fromDevice, CustomDataStream dataStream);
};


#endif //LANSHARE_FILESEND_H