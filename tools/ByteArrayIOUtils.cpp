//
// Created by fgsq on 2024/2/21.
//

#include "ByteArrayIOUtils.h"
#include <QtGlobal>

ByteArrayIOUtils::ByteArrayIOUtils(QByteArray array) {
    size = array.size();
    byteArray = new mbyte[size];
    memcpy(byteArray, array.data(), size);

}

mlong ByteArrayIOUtils::setSeek(mlong off) {
    // 确保偏移量在有效范围内
    if (off < 0 || off > size) {
        return -1; // 返回负值表示参数无效
    }
    // 更新当前读取位置
    currentPosition = off;
    return currentPosition; // 返回新的读取位置
}

mlong ByteArrayIOUtils::getSeek() {
    return currentPosition;
}

int ByteArrayIOUtils::read(void *buff, int len) {
    if (!buff || len <= 0) {
        return -1; // 返回负值表示参数无效
    }
    // 计算实际可读取的字节数，即剩余未读取的字节数和要读取的长度中的较小值
    mlong bytesToRead = qMin((mlong) len, size - currentPosition);
    if (bytesToRead <= 0) {
        return 0; // 已经读取完所有数据
    }
    // 将数据从字节数组复制到缓冲区
    memcpy(buff, byteArray + currentPosition, bytesToRead);
    // 更新当前读取位置
    currentPosition += bytesToRead;
    return (int) bytesToRead; // 返回实际读取的字节数
}

int ByteArrayIOUtils::write(const char *buff, int len) {
    return 0;
}

int64_t ByteArrayIOUtils::getFileSize() {
    return size;
}

void ByteArrayIOUtils::close() {

}

ByteArrayIOUtils::~ByteArrayIOUtils() {
    delete byteArray;
}

