//
// Created by fgsqme on 2024/5/13.
//

#include "ResourceIOUtils.h"

extern int openData(const std::string &resName);

extern int readData(int fd, char *buff, mlong offset, int len);

extern long getFileSize(int fd);

ResourceIOUtils::ResourceIOUtils(const QString &resName) {
    fd = openData(resName.toStdString());
}

mlong ResourceIOUtils::setSeek(mlong off) {
    this->offset = off;
    return off;
}

mlong ResourceIOUtils::getSeek() {
    return this->offset;
}

int ResourceIOUtils::read(void *buff, int len) {
    int i = readData(fd, (char *) buff, offset, len);
    this->offset += i;
    return i;
}

int64_t ResourceIOUtils::getFileSize() {
    return ::getFileSize(fd);
}

int ResourceIOUtils::write(const char *buff, int len) {
    return 0;
}

void ResourceIOUtils::close() {
    fd = -1;
}
