//
// Created by fgsqme on 2021/10/30.
//

#ifndef EP_IOINTER_H
#define EP_IOINTER_H

#include  <string>
#include  "Type.h"


/**
 * IO 接口 - 输入输出抽象基类
 * 定义了文件/流读写操作的统一接口
 * @author fgsq
 * @version 1.0
 */
class IOInter {

public:
    /** 析构函数 */
    virtual ~IOInter() = default;

    /**
     * 设置读写偏移位置
     * @param off 偏移量
     * @return 设置后的偏移位置，失败返回负数
     */
    virtual mlong setSeek(mlong off) = 0;

    /**
     * 获取当前读写偏移位置
     * @return 当前偏移位置
     */
    virtual mlong getSeek() = 0;

    /**
     * 读取数据到缓冲区
     * @param buff 目标缓冲区
     * @param len 期望读取的字节数
     * @return 实际读取的字节数
     */
    virtual int read(void *buff, int len) = 0;

    /**
     * 写入数据
     * @param buff 待写入数据
     * @param len 写入字节数
     * @return 实际写入的字节数
     */
    virtual int write(const char *buff, int len) = 0;

    /**
     * 获取文件/数据总大小
     * @return 文件大小（字节）
     */
    virtual int64_t getFileSize() = 0;

    /**
     * 关闭 IO 流
     */
    virtual void close() = 0;
};


#endif //EP_IOINTER_H
