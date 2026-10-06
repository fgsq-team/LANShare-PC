//
// Created by fgsqme on 2021/8/4 0004.
//
#include "TimeTools.h"
#if defined(PLATFORM_WINDOWS)
#include <sys/timeb.h>
#include <ctime>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <winsock.h>
#include <windows.h>
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#endif

std::string TimeTools::getFormatTime()
{
    char str_time[20];
    time_t now;
    struct tm *tm_now;
    char datetime[128];
    time(&now);
    tm_now = localtime(&now);
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
    struct timeval delay{0,us};
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

