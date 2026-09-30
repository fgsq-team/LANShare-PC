//
// Created by fgsqme on 2023/2/1.
//
extern bool OPEN_DEBUG;

#ifndef LANSHART_WIN_LOG_H
#define LANSHART_WIN_LOG_H


class LLog {

private:
    /*  0 = 黑色 8 = 灰色
        1 = 蓝色 9 = 淡蓝色
        2 = 绿色 10 = 淡绿色
        3 = 浅绿色 11 = 淡浅绿色
        4 = 红色 12 = 淡红色
        5 = 紫色 13 = 淡紫色
        6 = 黄色 14 = 淡黄色
        7 = 白色 15 = 亮白色
        */
    template<typename ...T>
    static void log(const std::string &str, int color, T...param) {
#if defined(PLATFORM_WINDOWS)
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | color);
        std::string newStr = str + "\n";
        qDebug(newStr.c_str(), param...);
        SetConsoleTextAttribute(handle, FOREGROUND_INTENSITY | 7);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
        qDebug(str.c_str(), param...);
#endif
    }

public:
    template<typename... T>
    static void Success(const std::string str, T... param) {
        log(str, 2, param...);
    }

    template<typename  ... T>
    static void Debug(const std::string str, T... param) {
        if (OPEN_DEBUG) {
            log(str, 7, param...);
        }
    }

    template<typename... T>
    static void Info(const std::string str, T... param) {
        log(str, 7, param...);
    }

    template<typename... T>
    static void Warning(const std::string str, T... param) {
        log(str, 6, param...);
    }

    template<typename... T>
    static void Error(const std::string str, T... param) {
        log(str, 4, param...);
    }

    template<typename T>
    const LLog &operator<<(T str) const {
        log(str, 7);
        return *this;
    }


};


#endif //LANSHART_WIN_LOG_H
