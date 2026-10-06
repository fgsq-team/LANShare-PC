//
// Created by 16508 on 2025/8/30.
//

#ifndef LANSHARE_FILESERVER_H
#define LANSHARE_FILESERVER_H
#include "CustomDataStream.h"
#include "ProgressCallback.h"

#define FILE_TYPE_IMAGE 0x1
#define FILE_TYPE_VIDEO 0x2
#define FILE_TYPE_AUDIO 0x3
#define FILE_TYPE_APK 0x4
#define FILE_TYPE_FILE 0x5
#define FILE_TYPE_FOLDER 0x6
#define FILE_TYPE_DOWNLOAD_INFO 0x7
#define FILE_TYPE_STREAM 0x8
#define FILE_TYPE_URI 0x9


class FileServer {
private:
    ProgressCallback *callback;

private:
    mlong recvStreamToFile(FileTransfer *fileTransfer, CustomDataStream *stream, mlong total, mlong finalSize,
                          LFile *baseFile, const LFile *fileItem, const QString &filePath) const;

    mlong recvStreamToFileDec(FileTransfer *fileTransfer, CustomDataStream *stream, mlong total, mlong finalSize,
                              LFile *baseFile, const LFile *fileItem, const QString &filePath) const;

public:
    FileServer();

    void handleFileTransfer(const Device &fromDevice, std::unique_ptr<TCPClient> tcpClient, CustomDataStream *stream);

    void handleVersion1(const Device &device, std::unique_ptr<TCPClient> tcpClient);

    void handleMediaSync(const Device &device, CustomDataStream *stream);

    void handleMessage(const Device &device, CustomDataStream *stream);

    void startReceiveFile(FileTransfer *fileTransfer, std::vector<LFile *> files,
                          CustomDataStream *stream,
                          boolean encData, boolean isAgree);

    void handleFolder(CustomDataStream *stream, FileTransfer *fileTransfer, LFile *fileItem, boolean encData) const;

    void handleFile(CustomDataStream *stream, FileTransfer *fileTransfer, LFile *fileItem, boolean encData);
};


#endif //LANSHARE_FILESERVER_H
