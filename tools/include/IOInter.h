//
// Created by fgsqme on 2021/10/30.
//

#ifndef EP_IOINTER_H
#define EP_IOINTER_H

#include  <string>
#include  "Type.h"


class IOInter {

public:
    virtual ~IOInter() = default;

    virtual mlong setSeek(mlong off) = 0;

    virtual mlong getSeek() = 0;

    virtual int read(void *buff, int len) = 0;

    virtual int write(const char *buff, int len) = 0;

    virtual int64_t getFileSize() = 0;

    virtual void close() = 0;
};


#endif //EP_IOINTER_H
