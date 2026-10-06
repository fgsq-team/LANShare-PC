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


/**
 * 文件 IO 工具类
 * 基于 QFile 封装文件读写操作，实现 IOInter 接口
 * @author fgsq
 * @version 1.0
 */
class IOUtils : public IOInter {
private:
    QFile *file;            // Qt 文件对象
    QString file_path;      // 文件路径
    mlong file_seek = 0;    // 当前读写位置
public:
    /**
     * 构造函数
     * @param str 文件路径
     * @param flags 打开模式，默认读写
     */
    explicit IOUtils(const QString& str, QFile::OpenMode flags = QFile::ReadWrite);

    /** 析构函数 */
    ~IOUtils() override;

    /**
     * 判断文件是否已打开
     * @return 是否已打开
     */
    bool isOpen();

    /**
     * 设置文件读写偏移
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
     * 写入数据
     * @param buff 待写入数据
     * @param len 写入字节数
     * @return 实际写入的字节数
     */
    int write(const char *buff, int len) override;

    /**
     * 写入数据（带偏移）
     * @param buff 待写入数据
     * @param index 数据偏移
     * @param len 写入字节数
     * @return 实际写入的字节数
     */
    int write(const char *buff, int index, int len);

    /**
     * 获取文件大小
     * @return 文件大小（字节）
     */
    int64_t getFileSize() override;

    /**
     * 关闭文件
     */
    void close() override;

    /**
     * 删除文件
     * @return 删除成功返回 true
     */
    bool deleteFile();

    /**
     * 获取文件路径
     * @return 文件路径引用
     */
    const QString &getFilePath() const;

};


#endif //EP_IOUTILS_H
