//
// Created by fgsqme on 2022/3/21 0021.
//

#ifndef LANSHARE_WIN_UTILS_H
#define LANSHARE_WIN_UTILS_H


#include <string>
#include <QString>
#include "LString.h"

/**
 * 通用工具类
 * 提供剪切板操作、音量控制、文件大小计算、UUID 生成等实用功能
 * @author fgsq
 * @version 1.0
 */
class Utils {
public:
    /**
     * 设置文本到系统剪切板
     * @param str 待设置文本
     * @return 成功返回 true
     */
    static bool SetClipboardText(const char *str);

    /**
     * 根据文件路径获取文件名称
     * @param path 文件路径
     * @return 文件名称
     */
    static std::string GetPathName(const std::string &path);

    /**
     * 将字节数转换为可读的文件大小字符串
     * @param size 字节数
     * @return 格式化字符串，如 "1.5MB"
     */
    static std::string computeSize(int64_t size);

    /**
     * 设置系统音量
     * @param level 音量级别（0-100），-1 静音，-2 取消静音
     * @return 成功返回 true
     */
    static bool setVolum(int level);

    /**
     * 获取系统音量
     * @return 音量级别（0-100）
     */
    static int volume();

    /**
     * 生成 UUID
     * @return 无横线的 UUID 字符串
     */
    static QString getUUID();

    /**
     * URL 解码
     * @param input 编码后的字符串
     * @return 解码后的字符串
     */
    static LString urlDecode(const LString &input);

    /**
     * 判断文件是否为图片
     * @param fileName 文件名称
     * @return 是图片返回 true
     */
    static bool isPhoto(const QString &fileName);
};


#endif //LANSHARE_WIN_UTILS_H
