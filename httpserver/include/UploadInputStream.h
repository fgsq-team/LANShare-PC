//
// Created by fgsq on 2023/12/19.
//

#ifndef LANSHARE_UPLOADINPUTSTREAM_H
#define LANSHARE_UPLOADINPUTSTREAM_H

#include <QList>
#include <QByteArray>
#include "IOInter.h"
#include "TCPClient.h"

class UploadInputStream : public IOInter {
public:
    ~UploadInputStream() override;

    UploadInputStream(TCPClient *is, qint64 contentLength);

    QString getFileName() const;

    int64_t getFileSize();

    qint64 getReadSize() const;

    mlong setSeek(mlong off);

    mlong getSeek();

    int write(const char *buff, int len);

    int read(void *buff, int len);

    void close() override;


private:
    QByteArray readHttpLineByte(TCPClient *is);

private:
    QString fileName;
    qint64 fileSize;
    qint64 readSize;
    TCPClient *is;
};

#endif //LANSHARE_UPLOADINPUTSTREAM_H
