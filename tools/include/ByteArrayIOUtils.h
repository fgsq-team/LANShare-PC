//
// Created by fgsq on 2024/2/21.
//

#ifndef LANSHARE_BYTEARRAYIOUTILS_H
#define LANSHARE_BYTEARRAYIOUTILS_H


#include <QByteArray>
#include "IOInter.h"

/**
 * QByteArray IO 工具类
 * 基于 QByteArray 封装读写操作，实现 IOInter 接口
 * @author fgsq
 * @version 1.0
 */
class ByteArrayIOUtils : public IOInter {
private:
    mbyte *byteArray;             // 数据缓冲区
    mlong currentPosition = 0;    // 当前读写位置
    mlong size = 0;               // 数据总大小
public:
    /**
     * 构造函数
     * @param array 源 QByteArray
     */
    explicit ByteArrayIOUtils(QByteArray array);

    /** 析构函数 */
    ~ByteArrayIOUtils() override;

public:
    /**
     * 设置读写偏移位置
     * @param off 偏移量
     * @return 设置后的偏移位置
     */
    mlong setSeek(mlong off) override;

    /**
     * 获取当前读写偏移
     * @return 当前偏移位置
     */
    mlong getSeek() override;

    /**
     * 读取数据到缓冲区
     * @param buff 目标缓冲区
     * @param len 期望读取的字节数
     * @return 实际读取的字节数
     */
    int read(void *buff, int len) override;

    /**
     * 写入数据（不支持，返回 0）
     */
    int write(const char *buff, int len) override;

    /**
     * 获取数据总大小
     * @return 数据大小（字节）
     */
    int64_t getFileSize() override;

    /**
     * 关闭 IO 流
     */
    void close() override;
};


#endif //LANSHARE_BYTEARRAYIOUTILS_H
