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
};

#endif //EW_TIMETOOLS_H
