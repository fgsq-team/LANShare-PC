//
// Created by fgsqme on 2021/8/4 0004.
//
#include "TimeTools.h"
#include <iomanip>
#if defined(PLATFORM_WINDOWS)
#include <sys/timeb.h>
#include <ctime>
#include <string>
#include <sstream>
#include <iostream>
#include <winsock.h>
#include <windows.h>
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

std::string TimeTools::getFormatTime() {
    char str_time[20];
    time_t now;
    char datetime[128];
    time(&now);
    const tm *tm_now = localtime(&now);
    strftime(datetime, 128, "%Y-%m-%d %H:%M:%S", tm_now);
    strcpy(str_time, datetime);
    return str_time;
}

void TimeTools::sleep_s(int s) {
#if defined(PLATFORM_WINDOWS)
    Sleep((s * 1000));
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    sleep(s);
#endif
}

void TimeTools::sleep_ms(int ms) {
#if defined(PLATFORM_WINDOWS)
    Sleep(ms);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    usleep(ms * 1000);
#endif
}

void TimeTools::sleep_us(int us) {
    struct timeval delay{0, us};
    select(0, nullptr, nullptr, nullptr, &delay);
}

mlong TimeTools::getCurrentTime() {
#if defined(PLATFORM_WINDOWS)
    timeb now{};
    ftime(&now);
    std::stringstream milliStream;
    milliStream << std::setw(3) << std::setfill('0') << std::right << now.millitm;
    std::stringstream secStream;
    secStream << now.time;
    std::string timeStr(secStream.str());
    timeStr.append(milliStream.str());
    mlong timeLong;
    std::stringstream transStream(timeStr);
    transStream >> timeLong;
    return timeLong;
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

time_t TimeTools::getCurrentTimestamp() {
    return time(nullptr);
}

// 判断是否是今天
bool TimeTools::isToday(time_t timestamp) {
    time_t now = std::time(nullptr);
    std::tm nowTm = *std::localtime(&now);
    std::tm timestampTm = *std::localtime(&timestamp);

    return (nowTm.tm_year == timestampTm.tm_year &&
            nowTm.tm_yday == timestampTm.tm_yday);
}

bool TimeTools::isMoreThanFiveMinutesAgo(time_t timestamp) {
    // 获取当前时间
    time_t currentTime = std::time(nullptr);
    // 计算输入时间与当前时间的差值，以秒为单位
    double differenceInSeconds = std::difftime(currentTime, timestamp);
    // 检查差值是否超过5分钟（300秒）
    return differenceInSeconds > 300;
}


// 判断是否是昨天
bool TimeTools::isYesterday(time_t timestamp) {
    time_t now = std::time(nullptr);
    std::tm nowTm = *std::localtime(&now);

    // 获取昨天的日期
    nowTm.tm_mday -= 1;
    std::mktime(&nowTm);

    std::tm timestampTm = *std::localtime(&timestamp);

    return (nowTm.tm_year == timestampTm.tm_year &&
            nowTm.tm_yday == timestampTm.tm_yday);
}

// 判断是否是当月
bool TimeTools::isCurrentMonth(time_t timestamp) {
    time_t now = std::time(nullptr);
    std::tm nowTm = *std::localtime(&now);
    std::tm timestampTm = *std::localtime(&timestamp);

    return (nowTm.tm_year == timestampTm.tm_year &&
            nowTm.tm_mon == timestampTm.tm_mon);
}

// 判断是否是当年
bool TimeTools::isCurrentYear(time_t timestamp) {
    time_t now = std::time(nullptr);
    std::tm nowTm = *std::localtime(&now);
    std::tm timestampTm = *std::localtime(&timestamp);

    return (nowTm.tm_year == timestampTm.tm_year);
}

// 格式化时间
std::string TimeTools::formatDate(time_t timestamp, const std::string& format) {
    std::tm tmTime = *std::localtime(&timestamp);
    std::ostringstream oss;
    oss << std::put_time(&tmTime, format.c_str());
    return oss.str();
}

// 主逻辑
std::string TimeTools::getTimeMessage(time_t timestamp) {
    if (isToday(timestamp)) {
        // 今天
        std::string timeOfDay;
        int hour = std::localtime(&timestamp)->tm_hour;
        if (hour >= 1 && hour < 6) {
            timeOfDay = "凌晨";
        } else if (hour >= 6 && hour < 12) {
            timeOfDay = "早上";
        } else if (hour >= 12 && hour < 14) {
            timeOfDay = "中午";
        } else if (hour >= 14 && hour < 18) {
            timeOfDay = "下午";
        } else {
            timeOfDay = "晚上";
        }
        return timeOfDay + formatDate(timestamp, "%H:%M");
    } else if (isYesterday(timestamp)) {
        // 昨天
        return "昨天" + formatDate(timestamp, "%H:%M");
    } else if (isCurrentMonth(timestamp) || isCurrentYear(timestamp)) {
        // 当月或当年
        return formatDate(timestamp, "%m月%d日 %H:%M");
    } else {
        // 其他年份
        return formatDate(timestamp, "%Y年%m月%d日 %H:%M");
    }
}