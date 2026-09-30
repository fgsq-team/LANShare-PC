//
// Created by fgsqme on 2021/8/4 0004.
//

#ifndef EW_TIMETOOLS_H
#define EW_TIMETOOLS_H

#include "Type.h"
#include <string>

class TimeTools {
public:
    static std::string getFormatTime();

    static void sleep_s(int s);

    static void sleep_ms(int ms);

    static void sleep_us(int us);

    static mlong getCurrentTime();

    static time_t getCurrentTimestamp();

    static bool isToday(time_t timestamp);

    static bool isMoreThanFiveMinutesAgo(time_t inputTime);

    static bool isYesterday(time_t timestamp);

    static bool isCurrentMonth(time_t timestamp);

    static bool isCurrentYear(time_t timestamp);

    static std::string formatDate(time_t timestamp, const std::string &format);

    static std::string getTimeMessage(time_t timestamp);
};

#endif //EW_TIMETOOLS_H
