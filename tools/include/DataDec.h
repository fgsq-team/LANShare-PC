//
// Created by fgsqme on 2021/5/10.
//



#ifndef WZ_CHEAT_DATADEC_H
#define WZ_CHEAT_DATADEC_H

#include <string>
#include "Type.h"


/**
 * 数据解包类
 * 从字节数组中按大端序解包各种数据类型
 * 数据包结构：[cmd(4)][count(4)][length(4)][data...]
 * @author fgsq
 * @version 1.0
 */
class DataDec {
private:
    static const int HEADER_LEN = 12;  // 包头长度
    mbyte *m_bytes = nullptr;          // 数据缓冲区
    int index = HEADER_LEN;            // 当前读取位置
    int m_byteLen = 0;                 // 数据总长度

public:
    /**
     * 获取命令字（包头偏移 0）
     * @return 命令字
     */
    int getCmd();

    /**
     * 获取字节型命令字（包头偏移 0）
     * @return 字节型命令字
     */
    mbyte getByteCmd();

    /**
     * 获取计数器（包头偏移 4）
     * @return 计数值
     */
    int getCount();

    /**
     * 获取数据长度（包头偏移 8）
     * @return 数据长度
     */
    int getLength();

    /** 默认构造函数 */
    DataDec();

    /**
     * 构造函数
     * @param bytes 数据缓冲区
     * @param bytelen 数据长度
     */
    DataDec(mbyte *bytes, int bytelen);

    /**
     * 设置数据
     * @param bytes 数据缓冲区
     * @param bytelens 数据长度
     */
    void setData(mbyte *bytes, int bytelens);

    /** 从当前位置读取 int */
    int getInt();
    /** 从当前位置读取 long */
    mlong getLong();
    /** 从当前位置读取 byte */
    mbyte getByte();
    /** 从当前位置读取 bool */
    bool getBool();

    /**
     * 从当前位置读取字符串
     * @return 新分配的字符串（调用者负责释放）
     */
    char *getStr();
    /**
     * 从当前位置读取字符串
     * @return std::string 副本
     */
    std::string getString();
    /**
     * 从当前位置读取字符串到指定缓冲区
     * @param buff 目标缓冲区
     */
    void getStr(char *buff);

    /**
     * 获取剩余字节
     * @return 新分配的字节数组（调用者负责释放）
     */
    mbyte *getSurplusBytes();

    /**
     * 获取剩余字节到指定缓冲区
     * @param buff 目标缓冲区
     */
    void getSurplusBytes(mbyte *buff);

    /** 从当前位置读取 float */
    float getFloat();
    /** 从当前位置读取 double */
    double getDouble();

    /** 从指定偏移读取 int */
    int getInt(int i);
    /** 从指定偏移读取 long */
    mlong getLong(int i);
    /** 从指定偏移读取 byte */
    mbyte getByte(int i);
    /**
     * 从指定偏移读取字符串
     * @return 新分配的字符串（调用者负责释放）
     */
    char *getStr(int i);
    /** 从指定偏移读取 float */
    float getFloat(int i);
    /** 从指定偏移读取 double */
    double getDouble(int i);

    /**
     * 获取包头大小
     * @return 包头长度（12 字节）
     */
    static int headerSize();
    /** 重置读取位置到数据起始 */
    void reset();
    /**
     * 偏移读取位置
     * @param off 相对数据区的偏移量
     */
    void skip(int off);

    /**
     * 获取当前数据索引（相对数据区）
     * @return 数据区偏移
     */
    int getDataIndex() const;

    /**
     * 设置数据索引
     * @param i 数据区偏移
     */
    void setDataIndex(int i);

    /**
     * 获取原始缓冲区指针
     * @return 缓冲区指针
     */
    mbyte *getBuffer();

    /** 解密包头 */
    void decHeader();

    /** 解密数据区 */
    void decData();

    /** 解密全部数据（包头 + 数据区） */
    void decAllData();

    /**
     * 读取 long，不足时返回默认值
     * @param def 默认值
     * @return long 值或默认值
     */
    mlong getLongDefualt(int i);
};


#endif //WZ_CHEAT_DATADEC_H
