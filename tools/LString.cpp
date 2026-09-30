// LString.cpp
#include "LString.h"
#include <cstring>
#include <cctype>
#include <iostream>

#include <sstream>

LString::LString() : length(0) {
    str = new char[1];
    str[0] = '\0'; // 添加终止符
}

LString::LString(const int size) {
    length = size;
    str = new char[length + 1];
    str[length] = '\0'; // 添加终止符
}

LString::LString(const char *s) {
    length = std::strlen(s);
    str = new char[length + 1];
    if (length > 0) {
        std::strcpy(str, s);
    }
    str[length] = '\0'; // 添加终止符
}

LString::LString(const LString &other) {
    length = other.length;
    str = new char[length + 1];
    if (length > 0) {
        std::strcpy(str, other.str);
    }
    str[length] = '\0'; // 添加终止符
}

LString::LString(const std::vector<uchar> &s) {
    length = s.size();
    str = new char[length + 1]; // 为字符串分配内存，长度为向量大小加上一个终止符 '\0'
    for (int i = 0; i < length; ++i) {
        str[i] = s[i]; // 将向量中的字符复制到字符串中
    }
    str[length] = '\0'; // 添加终止符
}

LString::LString(const std::vector<char> &s, int size) {
    length = size;
    str = new char[length + 1]; // 为字符串分配内存，长度为向量大小加上一个终止符 '\0'
    for (int i = 0; i < length; ++i) {
        str[i] = s[i]; // 将向量中的字符复制到字符串中
    }
    str[length] = '\0'; // 添加终止符
}

LString::LString(const char *s, int size) {
    length = size;
    str = new char[length + 1]; // 为字符串分配内存，长度为向量大小加上一个终止符 '\0'
    for (int i = 0; i < length; ++i) {
        str[i] = s[i]; // 将向量中的字符复制到字符串中
    }
    str[length] = '\0'; // 添加终止符
}

LString::LString(const LString &other, int size) {
    length = size;
    str = new char[length + 1]; // 为字符串分配内存，长度为向量大小加上一个终止符 '\0'
    for (int i = 0; i < length; ++i) {
        str[i] = other[i]; // 将向量中的字符复制到字符串中
    }
    str[length] = '\0'; // 添加终止符
}

LString::~LString() {
    delete[] str;
}

LString &LString::operator=(const LString &other) {
    if (this != &other) {
        delete[] str;
        length = other.length;
        str = new char[length + 1];
        std::strcpy(str, other.str);
    }
    return *this;
}

LString LString::operator+(const LString &other) const {
    LString temp;
    temp.length = length + other.length;
    temp.str = new char[temp.length + 1];
    std::strcpy(temp.str, str);
    std::strcat(temp.str, other.str);
    return temp;
}

// LString LString::operator+(const char *other) const {
//     if (!other) {
//         // 如果 other 为空指针，则返回当前字符串
//         return *this;
//     }
//
//     int otherLength = std::strlen(other);
//     LString temp;
//     temp.length = length + otherLength;
//     temp.str = new char[temp.length + 1];
//     std::strcpy(temp.str, str);
//     std::strcat(temp.str, other);
//     return temp;
// }

// LString &LString::operator+=(const char *other) {
//     if (!other) {
//         return *this; // 如果 other 为空指针，不进行操作，直接返回当前字符串
//     }
//     int otherLength = std::strlen(other);
//     char *newStr = new char[length + otherLength + 1]; // 新字符串的长度为两个字符串的长度之和，再加上终止符 '\0'
//     std::strcpy(newStr, str);                          // 复制当前字符串
//     std::strcat(newStr, other);                        // 连接其他字符串
//     delete[] str;                                      // 释放原来的字符串内存
//     str = newStr;                                      // 更新指针指向新字符串
//     length += otherLength;                             // 更新字符串长度
//     return *this;
// }
LString &LString::operator+=(const LString &other) {
    int newLength = length + other.length;
    char *newStr = new char[newLength + 1]; // 新字符串的长度为两个字符串的长度之和，再加上终止符 '\0'
    std::strcpy(newStr, str);               // 复制当前字符串
    std::strcat(newStr, other.str);         // 连接其他字符串
    delete[] str;                           // 释放原来的字符串内存
    str = newStr;                           // 更新指针指向新字符串
    length = newLength;                     // 更新字符串长度
    return *this;
}

LString &LString::operator+=(const char other) {
    char *newStr = new char[length + 2]; // 新字符串的长度为当前字符串的长度加上一个字符和终止符 '\0'
    if (length > 0) {
        std::strcpy(newStr, str); // 复制当前字符串
    }
    newStr[length] = other;    // 添加新字符
    newStr[length + 1] = '\0'; // 添加终止符
    delete[] str;              // 释放原来的字符串内存
    str = newStr;              // 更新指针指向新字符串
    ++length;                  // 更新字符串长度
    return *this;
}

LString &LString::operator<<(const LString &other) {
    int newLength = length + other.length;
    char *newStr = new char[newLength + 1]; // 新字符串的长度为两个字符串的长度之和，再加上终止符 '\0'
    std::strcpy(newStr, str);               // 复制当前字符串
    std::strcat(newStr, other.str);         // 连接其他字符串
    delete[] str;                           // 释放原来的字符串内存
    str = newStr;                           // 更新指针指向新字符串
    length = newLength;                     // 更新字符串长度
    return *this;
}

LString &LString::operator<<(const char other) {
    char *newStr = new char[length + 2]; // 新字符串的长度为当前字符串的长度加上一个字符和终止符 '\0'
    if (length > 0) {
        std::strcpy(newStr, str); // 复制当前字符串
    }
    newStr[length] = other;    // 添加新字符
    newStr[length + 1] = '\0'; // 添加终止符
    delete[] str;              // 释放原来的字符串内存
    str = newStr;              // 更新指针指向新字符串
    ++length;                  // 更新字符串长度
    return *this;
}

char &LString::operator[](int index) {
    return str[index];
}

const char &LString::operator[](int index) const {
    return str[index];
}

LString operator+(const char *lhs, const LString &rhs) {
    return LString(lhs) + rhs;
}

bool LString::operator==(const LString &other) const {
    // 实现比较逻辑，比如比较字符串内容是否相等
    return strcmp(str, other.str) == 0;
}

bool LString::operator<(const LString &other) const {
    // 实现比较逻辑，比如比较字符串内容的字典顺序
    return strcmp(str, other.str) < 0;
}

bool LString::operator>(const LString &other) const {
    // 实现比较逻辑，比如比较字符串内容的字典顺序
    return strcmp(other.str, str) < 0;
}

LString LString::formatNumber(int number) {
    std::ostringstream oss;
    oss << number;
    return {oss.str().c_str()};
}

LString LString::formatNumber(long long number) {
    std::ostringstream oss;
    oss << number;
    return {oss.str().c_str()};
}

LString LString::formatNumber(float number) {
    std::ostringstream oss;
    oss << number;
    return {oss.str().c_str()};
}

LString LString::formatNumber(double number) {
    std::ostringstream oss;
    oss << number;
    return {oss.str().c_str()};
}

std::vector<uchar> LString::toByteArray() const {
    return {str, str + length};
}

int LString::getLength() const {
    return length;
}

char *LString::getCString() const {
    return str;
}

std::string LString::getStdString() const {
    return str;
}

bool LString::startsWith(const LString &prefix) const {
    if (length < prefix.length) {
        return false;
    }
    for (int i = 0; i < prefix.length; ++i) {
        if (str[i] != prefix.str[i]) {
            return false;
        }
    }
    return true;
}

bool LString::equals(const LString &other) const {
    if (length != other.length) {
        return false;
    }
    return std::strcmp(str, other.str) == 0;
}

LString LString::toLowerCase() const {
    LString result(*this); // 创建一个新对象，使用当前对象的内容进行初始化
    for (int i = 0; i < result.length; ++i) {
        result.str[i] = (char) std::tolower(result.str[i]);
    }
    return result;
}

LString LString::toUpperCase() const {
    LString result(*this); // 创建一个新对象，使用当前对象的内容进行初始化
    for (int i = 0; i < result.length; ++i) {
        result.str[i] = (char) std::toupper(result.str[i]);
    }
    return result;
}

LString LString::substring(int startIndex) const {
    return substring(startIndex, length - 1);
}

LString LString::substring(int startIndex, int endIndex) const {
    if (startIndex < 0 || startIndex >= length || endIndex < 0 || endIndex >= length || startIndex > endIndex) {
        return {};
    }
    int subLength = endIndex - startIndex + 1;
    char *subStr = new char[subLength + 1];
    std::strncpy(subStr, str + startIndex, subLength);
    subStr[subLength] = '\0';
    return {subStr};
}

int LString::indexOf(const LString &subStr) const {
    for (int i = 0; i <= length - subStr.length; ++i) {
        bool found = true;
        for (int j = 0; j < subStr.length; ++j) {
            if (str[i + j] != subStr.str[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }
    return -1;
}

int LString::indexOf(const LString &subStr, int startIndex) const {
    for (int i = startIndex; i <= length - subStr.length; ++i) {
        bool found = true;
        for (int j = 0; j < subStr.length; ++j) {
            if (str[i + j] != subStr.str[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }
    return -1;
}

int LString::lastIndexOf(const LString &subStr) const {
    for (int i = length - subStr.length; i >= 0; --i) {
        bool found = true;
        for (int j = 0; j < subStr.length; ++j) {
            if (str[i + j] != subStr.str[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            return i;
        }
    }
    return -1;
}

int LString::lastIndexOf(const char subChar) const {
    for (int i = length - 1; i >= 0; --i) {
        if (str[i] == subChar) {
            return i;
        }
    }
    return -1;
}

std::vector<LString> LString::split(const LString &delimiter) const {
    std::vector<LString> result;
    int startIndex = 0;
    int delimiterIndex;

    while ((delimiterIndex = indexOf(delimiter, startIndex)) != -1) {
        result.push_back(substring(startIndex, delimiterIndex - 1)); // 将子字符串添加到结果向量中
        startIndex = delimiterIndex + delimiter.length;              // 更新起始索引
    }

    // 添加最后一个子字符串（可能没有分隔符）
    result.push_back(substring(startIndex));

    return result;
}

std::vector<LString> LString::split(char delimiter) const {
    std::vector<LString> result;
    int start = 0;
    for (int i = 0; i < length; ++i) {
        if (str[i] == delimiter) {
            result.push_back(substring(start, i - 1));
            start = i + 1;
        }
    }
    // 添加最后一个子字符串（可能没有分隔符）
    result.push_back(substring(start, length - 1));
    return result;
}

long long LString::toLongLong() const {
    return std::strtol(str, nullptr, 10);
}

int LString::compare(const LString &other) const {
    return std::strcmp(str, other.str);
}

bool LString::contains(const LString &subStr) const {
    return std::strstr(str, subStr.str) != nullptr;
}

LString LString::remove(int startIndex) const {
    return remove(startIndex, length - 1);
}

LString LString::remove(int startIndex, int count) const {
    if (startIndex < 0 || startIndex >= length || count <= 0) {
        // 如果起始索引超出范围或者删除的字符数为非正数，则直接返回原字符串
        return *this;
    }
    if (startIndex + count > length) {
        // 如果删除的字符数超出范围，则将 count 调整为剩余字符数
        count = length - startIndex;
    }
    // 创建一个新的字符串，长度为原字符串长度减去删除的字符数
    LString result;
    result.length = length - count;
    result.str = new char[result.length + 1];

    // 复制要保留的字符到新的字符串中
    std::memcpy(result.str, str, startIndex);
    std::memcpy(result.str + startIndex, str + startIndex + count, length - startIndex - count);
    result.str[result.length] = '\0'; // 添加终止符
    return result;
}

LString LString::remove(const LString &subStr) const {
    int index = indexOf(subStr); // 找到子字符串的起始位置
    if (index == -1) {
        // 如果当前字符串不包含子字符串，则直接返回原字符串
        return *this;
    }
    // 创建一个新的字符串，长度为原字符串长度减去子字符串长度
    LString result;
    result.length = length - subStr.length;
    result.str = new char[result.length + 1];
    // 复制要保留的字符到新的字符串中
    std::memcpy(result.str, str, index);
    std::memcpy(result.str + index, str + index + subStr.length, length - index - subStr.length);
    result.str[result.length] = '\0'; // 添加终止符
    return result;
}

void LString::replace(const LString &oldSubStr, const LString &newSubStr) {
    int index = indexOf(oldSubStr); // 找到要替换的子字符串的位置
    if (index != -1) {
        // 创建一个新的字符串，长度为原字符串长度减去旧子字符串长度，加上新子字符串长度
        int newLength = length - oldSubStr.length + newSubStr.length;
        char *newStr = new char[newLength + 1];
        // 复制旧子字符串之前的部分到新字符串中
        std::memcpy(newStr, str, index);
        // 复制新子字符串到新字符串中
        std::memcpy(newStr + index, newSubStr.str, newSubStr.length);
        // 复制旧子字符串之后的部分到新字符串中
        std::memcpy(newStr + index + newSubStr.length, str + index + oldSubStr.length,
                    length - index - oldSubStr.length);
        // 添加终止符
        newStr[newLength] = '\0';
        // 释放原来的字符串内存，并更新指针指向新字符串
        delete[] str;
        str = newStr;
        length = newLength;
    }
}

void LString::append(int count, char c) {
    // 检查 count 是否为非负数
    if (count < 0) {
        return;
    }
    // 创建一个新的字符串，长度为原字符串长度加上 count
    char *newStr = new char[length + count + 1];
    // 复制原字符串到新字符串中
    std::strcpy(newStr, str);
    // 追加指定数量的字符 c
    for (int i = 0; i < count; ++i) {
        newStr[length + i] = c;
    }
    // 添加终止符
    newStr[length + count] = '\0';
    // 释放原来的字符串内存，并更新指针指向新字符串
    delete[] str;
    str = newStr;
    // 更新字符串长度
    length += count;
}

bool LString::isEmpty() const {
    return length == 0;
}