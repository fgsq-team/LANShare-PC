//
// Created by fgsqme on 2021/8/4 0004.
//

#ifndef EW_TIMETOOLS_H
#define EW_TIMETOOLS_H

#include "Type.h"
#include <string>

/**
 * 时间工具类
 * 提供时间获取、延时等跨平台操作
 * @author fgsq
 * @version 1.0
 */
class TimeTools {
public:
    /**
     * 获取格式化的当前时间字符串
     * @return 格式为 "YYYY-MM-DD HH:MM:SS" 的时间字符串
     */
    static std::string getFormatTime();

    /**
     * 休眠指定秒数
     * @param s 秒数
     */
    static void sleep_s(int s);

    /**
     * 休眠指定毫秒数
     * @param ms 毫秒数
     */
    static void sleep_ms(int ms);

    /**
     * 休眠指定微秒数
     * @param us 微秒数
     */
    static void sleep_us(int us);

    /**
     * 获取当前时间戳（毫秒级）
     * @return 毫秒级时间戳
     */
    static mlong getCurrentTime();
};

#endif //EW_TIMETOOLS_H
