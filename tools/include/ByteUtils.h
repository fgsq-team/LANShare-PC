//
// Created by fgsqme on 2021/5/10.
//
#include "Type.h"

#ifndef WZ_CHEAT_BYTEUTIL_H
#define WZ_CHEAT_BYTEUTIL_H


/**
 * 字节工具类
 * 提供基本数据类型与字节数组之间的转换操作
 * @author fgsq
 * @version 1.0
 */
class ByteUtils {
public:
    /**
     * 复制字节数组
     * @param d 源数组
     * @param d_index 源偏移
     * @param t 目标数组
     * @param t_index 目标偏移
     * @param length 复制长度
     */
    static void ByteArrCopy(const mbyte *d, int d_index, mbyte *t, int t_index, int length);

    /**
     * int 转字节数组（大端序）
     * @param i 整数值
     * @param b 目标字节数组
     * @param index 写入偏移，默认 0
     */
    static void intToBytes(int i, mbyte *b, int index = 0);

    /**
     * 字节数组转 int（大端序）
     * @param buf 源字节数组
     * @param offset 读取偏移
     * @return 整数值
     */
    static int bytesToInt(mbyte *buf, int offset);

    /**
     * long 转字节数组（大端序）
     * @param i long 值
     * @param b 目标字节数组
     * @param index 写入偏移，默认 0
     */
    static void longToBytes(mlong i, mbyte *b, int index = 0);

    /**
     * 字节数组转 long（大端序）
     * @param buf 源字节数组
     * @param offset 读取偏移，默认 0
     * @return long 值
     */
    static mlong bytesToLong(mbyte *buf, int offset = 0);

    /**
     * 字节数组转 short（大端序）
     * @param buf 源字节数组
     * @param offset 读取偏移，默认 0
     * @return short 值
     */
    static short bytesToShort(const mbyte *buf, int offset = 0);

    /**
     * short 转字节数组（大端序）
     * @param i short 值
     * @param b 目标字节数组
     */
    static void shortToBytes(short i, mbyte *b);
};


#endif //WZ_CHEAT_BYTEUTIL_H
