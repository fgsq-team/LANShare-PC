//
// Created by fgsqme on 2021/9/19.
//

#include "IOUtils.h"
#include "DataEnc.h"

/**
 * 析构函数，关闭并释放文件
 */
IOUtils::~IOUtils() {
    file->close();
    delete file;
}

/**
 * 构造函数，打开文件
 */
IOUtils::IOUtils(const QString &path, QFile::OpenMode flags) {
    file_path = path;
    file = new QFile(path);
    file->open(flags);
}

/**
 * 设置文件读写偏移
 */
mlong IOUtils::setSeek(mlong off) {

    if (file->seek(off)) {
        return file_seek = off;
    }
    return -1;
}

/**
 * 获取当前读写偏移
 */
mlong IOUtils::getSeek() {
    return file_seek;
}

/**
 * 读取数据到缓冲区
 */
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

/**
 * 写入数据
 */
int IOUtils::write(const char *buff, int len) {
    return write(buff, 0, len);
}

/**
 * 写入数据（带偏移）
 */
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

/**
 * 关闭文件
 */
void IOUtils::close() {
    file->close();
}

/**
 * 获取文件大小
 */
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

/**
 * 判断文件是否已打开
 */
bool IOUtils::isOpen() {
    return file->isOpen();
}

/**
 * 获取文件路径
 */
const QString &IOUtils::getFilePath() const {
    return file_path;
}

/**
 * 删除文件
 */
bool IOUtils::deleteFile() {
    return file->remove();
}





