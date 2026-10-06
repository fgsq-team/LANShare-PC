//
// Created by fgsqme on 2021/5/10.
//

#include "Type.h"
#include <string>
#include <QString>

#ifndef WZ_CHEAT_DATAENC_H
#define WZ_CHEAT_DATAENC_H

/**
 * 数据打包类
 * 将各种数据类型按大端序编码到字节数组中
 * 数据包结构：[cmd(4)][count(4)][length(4)][data...]
 * @author fgsq
 * @version 1.0
 */
class DataEnc {
private:
    static const int HEADER_LEN = 12;  // 包头长度

    mbyte *m_bytes = nullptr;          // 数据缓冲区
    int index = HEADER_LEN;            // 当前写入位置
    int m_byteLen = 0;                 // 缓冲区总长度
    boolean dataEncrypted = false;     // 是否已加密

public:
    /** 默认构造函数 */
    DataEnc();

    /**
     * 构造函数
     * @param bytes 数据缓冲区
     * @param bytelen 缓冲区长度
     */
    DataEnc(mbyte *bytes, int bytelen);

    /**
     * 设置数据缓冲区
     * @param bytes 数据缓冲区
     * @param bytelen 缓冲区长度
     */
    void setData(mbyte *bytes, int bytelen);

    /**
     * 设置命令字（包头偏移 0）
     * @param cmd 命令字
     */
    void setCmd(int cmd);

    /**
     * 设置字节型命令字（包头偏移 0）
     * @param cmd 命令字
     */
    void setByteCmd(mbyte cmd);

    /**
     * 设置计数器（包头偏移 4）
     * @param count 计数值
     */
    void setCount(int count);

    /**
     * 设置数据长度（包头偏移 8）
     * @param len 数据长度
     */
    void setLength(int len);


    /** 追加 int 到数据区 */
    DataEnc &putInt(int val);
    /** 追加 long 到数据区 */
    DataEnc &putLong(mlong val);
    /** 追加 byte 到数据区 */
    DataEnc &putByte(mbyte val);
    /** 追加 bool 到数据区 */
    DataEnc &putBool(bool val);
    /** 追加 float 到数据区（乘以 1000 转 int 存储） */
    DataEnc &putFloat(float val);
    /** 追加 double 到数据区（乘以 1000000 转 long 存储） */
    DataEnc &putDouble(double val);
    /** 追加字符串到数据区（带长度前缀） */
    DataEnc &putStr(const char *str, int len);
    /** 追加字符串到数据区（带长度前缀） */
    DataEnc &putStr(const char *str);
    /** 追加 std::string 到数据区 */
    DataEnc &putString(const std::string &str);
    /** 追加 QString 到数据区 */
    DataEnc &putString(const QString &str);

    /** 在指定偏移写入 int */
    DataEnc &putInt(int val, int i);
    /** 在指定偏移写入 long */
    DataEnc &putLong(mlong val, int i);
    /** 在指定偏移写入 byte */
    DataEnc &putByte(mbyte val, int i);
    /** 在指定偏移写入 float */
    DataEnc &putFloat(float val, int i);
    /** 在指定偏移写入 double */
    DataEnc &putDouble(double val, int i);
    /** 在指定偏移写入字符串 */
    DataEnc &putStr(const char *str, int len, int i);

    /**
     * 获取当前数据区长度（不含包头）
     * @return 数据区字节数
     */
    int getDataIndex() const;

    /**
     * 获取已写入的总长度（含包头）
     * @return 总字节数
     */
    int getDataLen() const;

    /**
     * 获取打包后的数据（自动填充长度字段）
     * @return 数据缓冲区指针
     */
    mbyte *getData();

    /**
     * 获取原始缓冲区指针
     * @return 缓冲区指针
     */
    mbyte *getBuffer();

    /** 重置写入位置到数据区起始 */
    void reset();

    /**
     * 获取包头大小
     * @return 包头长度（12 字节）
     */
    static int headerSize();

    /**
     * 设置数据索引
     * @param i 数据区偏移
     */
    void setDataIndex(int i);

    /**
     * 加密并返回数据
     * 对包头和数据区进行异或加密
     * @return 加密后的数据缓冲区指针
     */
    mbyte *encData();
};


#endif //WZ_CHEAT_DATAENC_H
