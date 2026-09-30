//
// Created by fgsqme on 2021/9/19.
//

#include "IOUtils.h"
#include "DataEnc.h"

IOUtils::~IOUtils() {
    file->close();
    delete file;
}

IOUtils::IOUtils(const QString &path, QFile::OpenMode flags) {
    file_path = path;
    file = new QFile(path);
    file->open(flags);
}

mlong IOUtils::setSeek(mlong off) {

    if (file->seek(off)) {
        return file_seek = off;
    }
    return -1;
}

mlong IOUtils::getSeek() {
    return file_seek;
}

int IOUtils::read(void *buff, int len) {
    if (len <= 0) {
        qDebug("read len < 0");
        return -3;
    }
    int ten = (int) file->read((char *) buff, len);
    if (ten > 0) {
        file_seek += ten;
    }
    return ten;
}

int IOUtils::write(const char *buff, int len) {
    return write(buff, 0, len);
}

int IOUtils::write(const char *buff, int index, int len) {
    if (len <= 0) {
        qDebug("write len < 0");
        return -3;
    }
    int ten = (int) file->write(&buff[index], len);
    if (ten > 0) {
        file_seek += ten;
    }
    return ten;
}

void IOUtils::close() {
    file->close();
}

int64_t IOUtils::getFileSize() {
    return file->size();
}

/*
int64_t IOUtils::getFileSize(const char *path) {
    FILE *file;
    fopen_s(&file, path, "r");
    if (file) {
        int size = _filelength(_fileno(file));
        fclose(file);
        return size;
    }
    return -1;
}*/

bool IOUtils::isOpen() {
    return file->isOpen();
}

const QString &IOUtils::getFilePath() const {
    return file_path;
}

bool IOUtils::deleteFile() {
    return file->remove();
}





