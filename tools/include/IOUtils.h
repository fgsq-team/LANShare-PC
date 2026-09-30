//
// Created by fgsqme on 2021/9/19.
//

#ifndef EP_IOUTILS_H
#define EP_IOUTILS_H

#include  <string>
#include  <QString>
#include  <QFile>
#include  "Type.h"
#include  "IOInter.h"
#include "DataEnc.h"


class IOUtils : public IOInter {
private:
    QFile *file;
    QString file_path;
    mlong file_seek = 0;
public:
    explicit IOUtils(const QString& str, QFile::OpenMode flags = QFile::ReadWrite);

    ~IOUtils() override;

    bool isOpen();

    mlong setSeek(mlong off) override;

    mlong getSeek() override;

    int read(void *buff, int len) override;

    int write(const char *buff, int len) override;

    int write(const char *buff, int index, int len);

    int64_t getFileSize() override;

    void close() override;

    bool deleteFile();

    const QString &getFilePath() const;

};


#endif //EP_IOUTILS_H
