//
// Created by fgsqme on 2022/3/20 0020.
//

#ifndef LANSHARE_WIN_CODEUTILS_H
#define LANSHARE_WIN_CODEUTILS_H

#include <string>
//#include <windows.h>
#include <QString>
#include "Type.h"

/**
 * 编码工具类
 * 提供字符编码转换和字符串长度计算等操作
 * @author fgsq
 * @version 1.0
 */
class CodeUtils {
public:
    /**
     * UTF-8 转 GBK（已废弃，返回 nullptr）
     * @param srcStr 源字符串
     * @return nullptr
     */
    static char* UTFToGBK(const QString& srcStr);

    /**
     * 计算 GBK 字符串的字符数
     * @param str GBK 编码的字符串
     * @return 字符数
     */
    static int getGbkStrLen(const char *str);

    /**
     * 计算 UTF-8 字符串的字符数
     * @param str UTF-8 编码的字符串
     * @return 字符数
     */
    static int getUtf8StrLen(const char *str);
};


#endif //LANSHARE_WIN_CODEUTILS_H
