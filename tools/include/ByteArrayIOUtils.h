//
// Created by fgsq on 2024/2/21.
//

#ifndef LANSHARE_BYTEARRAYIOUTILS_H
#define LANSHARE_BYTEARRAYIOUTILS_H


#include <QByteArray>
#include "IOInter.h"

class ByteArrayIOUtils : public IOInter {
private:
    mbyte *byteArray;
    mlong currentPosition = 0;
    mlong size = 0;
public:
    explicit ByteArrayIOUtils(QByteArray array);

    ~ByteArrayIOUtils() override;

public:
    mlong setSeek(mlong off) override;

    mlong getSeek() override;

    int read(void *buff, int len) override;

    int write(const char *buff, int len) override;

    int64_t getFileSize() override;

    void close() override;
};


#endif //LANSHARE_BYTEARRAYIOUTILS_H
