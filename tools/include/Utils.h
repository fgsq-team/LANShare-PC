//
// Created by fgsqme on 2022/3/21 0021.
//

#ifndef LANSHARE_WIN_UTILS_H
#define LANSHARE_WIN_UTILS_H


#include <string>
#include <QString>
#include "LString.h"

class Utils {
public:
    // 写入剪切板
    static bool SetClipboardText(const char *str);
    // 模拟键盘输入
//    static void SendKeys(const std::basic_string<char> &msg);
    // 根据文件路径获取文件名称
    static std::string GetPathName(const std::string &path);

    static std::string computeSize(int64_t size);

    static bool setVolum(int level);

    static int volume();

    static QString getUUID();

    static LString urlDecode(const LString &input);

    static bool isPhoto(const QString &fileName);
};


#endif //LANSHARE_WIN_UTILS_H
