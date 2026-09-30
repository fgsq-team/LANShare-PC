//
// Created by fgsq on 2024/2/22.
//

#ifndef LANSHARE_STRING_H
#define LANSHARE_STRING_H

#include "Type.h"
#include <vector>
#include <string>

class LString {
private:
    char *str;  // 用于存储字符串的字符数组
    int length; // 字符串的长度

public:
    LString();
    LString(const int size);
    LString(const char *s);
    LString(const LString &other);
    LString(const LString &other, int size);
    LString(const std::vector<uchar> &s);           // 支持std::vector<char>
    LString(const std::vector<char> &s, int size); // 支持std::vector<char>
    LString(const char *s, int size);              // 支持std::vector<char>

    ~LString();

    LString &operator=(const LString &other);
    LString operator+(const LString &other) const;
    //    LString operator+(const char *other) const;
    //    LString &operator+=(const char *other);
    LString &operator+=(const char other);
    LString &operator+=(const LString &other);

    LString &operator<<(const char other);
    LString &operator<<(const LString &other);

    //    LString operator+(const char *lhs, const LString &rhs);
    friend LString operator+(const char *lhs, const LString &rhs);
    bool operator==(const LString &other) const;
    bool operator<(const LString &other) const;
    bool operator>(const LString &other) const;

    char &operator[](int index);
    const char &operator[](int index) const;

    static LString formatNumber(int number);
    static LString formatNumber(long long number);
    static LString formatNumber(float number);
    static LString formatNumber(double number);
    std::vector<uchar> toByteArray() const;
    int getLength() const;
    char *getCString() const;
    std::string getStdString() const;

    bool startsWith(const LString &prefix) const;
    bool equals(const LString &other) const;
    LString toLowerCase() const;
    LString toUpperCase() const;

    LString substring(int startIndex, int endIndex) const;
    LString substring(int startIndex) const;

    int indexOf(const LString &subStr) const;
    int indexOf(const LString &subStr, int startIndex) const;
    int lastIndexOf(const LString &subStr) const;
    int lastIndexOf(const char subStr) const;
    std::vector<LString> split(char delimiter) const;
    std::vector<LString> split(const LString& delimiter) const;
    long long toLongLong() const;
    int compare(const LString &other) const;
    bool contains(const LString &subStr) const;
    LString remove(int startIndex, int count) const;
    LString remove(int startIndex) const;
    LString remove(const LString &subStr) const;
    void replace(const LString &oldSubStr, const LString &newSubStr);
    void append(int count, char c);
    bool isEmpty() const;
};

#endif // LANSHARE_STRING_H
