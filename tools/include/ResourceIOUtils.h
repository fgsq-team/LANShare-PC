//
// Created by fgsqme on 2024/5/13.
//

#ifndef LANSHARE_RESOURCEIOUTILS_H
#define LANSHARE_RESOURCEIOUTILS_H

#include <cstdint>
#include <QString>
#include "IOInter.h"

class ResourceIOUtils : public IOInter {
public:
    explicit ResourceIOUtils(const QString &resName);

    ~ResourceIOUtils() override = default;

    mlong setSeek(mlong off) override;

    mlong getSeek() override;

    int read(void *buff, int len) override;

    int write(const char *buff, int len) override;

    int64_t getFileSize() override;

    void close() override;

private:
    int fd;
    long offset = 0;
};


#endif //LANSHARE_RESOURCEIOUTILS_H
