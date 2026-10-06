// LString.h
// Created by fgsq on 2024/2/22.

#ifndef LANSHARE_STRING_H
#define LANSHARE_STRING_H

#include "Type.h"
#include <vector>
#include <string>

/**
 * 自定义字符串类
 * 提供类似 Java String 的字符串操作接口
 * @author fgsq
 * @version 1.0
 */
class LString {
private:
    char *str;   // 字符串字符数组
    int length;  // 字符串长度

public:
    /** 默认构造函数，创建空字符串 */
    LString();
    /** 构造指定长度的空字符串 */
    LString(const int size);
    /** 从 C 字符串构造 */
    LString(const char *s);
    /** 拷贝构造函数 */
    LString(const LString &other);
    /** 从另一个 LString 截取指定长度构造 */
    LString(const LString &other, int size);
    /** 从 uchar 向量构造 */
    LString(const std::vector<uchar> &s);
    /** 从 char 向量构造 */
    LString(const std::vector<char> &s, int size);
    /** 从 C 字符串截取指定长度构造 */
    LString(const char *s, int size);

    /** 析构函数 */
    ~LString();

    /** 赋值运算符 */
    LString &operator=(const LString &other);
    /** 字符串拼接 */
    LString operator+(const LString &other) const;
    /** 追加字符 */
    LString &operator+=(const char other);
    /** 追加字符串 */
    LString &operator+=(const LString &other);

    /** 追加字符（流式） */
    LString &operator<<(const char other);
    /** 追加字符串（流式） */
    LString &operator<<(const LString &other);

    /** C 字符串 + LString */
    friend LString operator+(const char *lhs, const LString &rhs);
    /** 内容相等比较 */
    bool operator==(const LString &other) const;
    /** 字典序小于比较 */
    bool operator<(const LString &other) const;
    /** 字典序大于比较 */
    bool operator>(const LString &other) const;

    /** 下标访问（可变） */
    char &operator[](int index);
    /** 下标访问（只读） */
    const char &operator[](int index) const;

    /** 将 int 格式化为字符串 */
    static LString formatNumber(int number);
    /** 将 long long 格式化为字符串 */
    static LString formatNumber(long long number);
    /** 将 float 格式化为字符串 */
    static LString formatNumber(float number);
    /** 将 double 格式化为字符串 */
    static LString formatNumber(double number);

    /** 转换为 uchar 向量 */
    std::vector<uchar> toByteArray() const;
    /** 获取字符串长度 */
    int getLength() const;
    /** 获取 C 字符串指针 */
    char *getCString() const;
    /** 转换为 std::string */
    std::string getStdString() const;

    /**
     * 判断是否以指定前缀开头
     * @param prefix 前缀字符串
     * @return 匹配返回 true
     */
    bool startsWith(const LString &prefix) const;

    /**
     * 判断内容是否相等
     * @param other 比较对象
     * @return 相等返回 true
     */
    bool equals(const LString &other) const;

    /** 转换为小写 */
    LString toLowerCase() const;
    /** 转换为大写 */
    LString toUpperCase() const;

    /**
     * 截取子字符串
     * @param startIndex 起始位置
     * @param endIndex 结束位置（包含）
     * @return 子字符串
     */
    LString substring(int startIndex, int endIndex) const;
    /**
     * 截取子字符串（从起始位置到末尾）
     * @param startIndex 起始位置
     * @return 子字符串
     */
    LString substring(int startIndex) const;

    /**
     * 查找子字符串首次出现的位置
     * @param subStr 待查找子字符串
     * @return 位置索引，未找到返回 -1
     */
    int indexOf(const LString &subStr) const;
    /**
     * 从指定位置开始查找子字符串首次出现的位置
     * @param subStr 待查找子字符串
     * @param startIndex 起始搜索位置
     * @return 位置索引，未找到返回 -1
     */
    int indexOf(const LString &subStr, int startIndex) const;
    /**
     * 查找子字符串最后出现的位置
     * @param subStr 待查找子字符串
     * @return 位置索引，未找到返回 -1
     */
    int lastIndexOf(const LString &subStr) const;
    /**
     * 查找字符最后出现的位置
     * @param subChar 待查找字符
     * @return 位置索引，未找到返回 -1
     */
    int lastIndexOf(const char subChar) const;

    /**
     * 按字符分隔符分割字符串
     * @param delimiter 分隔符
     * @return 分割后的字符串数组
     */
    std::vector<LString> split(char delimiter) const;
    /**
     * 按字符串分隔符分割字符串
     * @param delimiter 分隔符
     * @return 分割后的字符串数组
     */
    std::vector<LString> split(const LString& delimiter) const;

    /** 转换为 long long */
    long long toLongLong() const;
    /**
     * 字典序比较
     * @param other 比较对象
     * @return 负数/0/正数
     */
    int compare(const LString &other) const;
    /**
     * 判断是否包含子字符串
     * @param subStr 待查找子字符串
     * @return 包含返回 true
     */
    bool contains(const LString &subStr) const;

    /**
     * 删除指定范围的字符
     * @param startIndex 起始位置
     * @param count 删除字符数
     * @return 新字符串
     */
    LString remove(int startIndex, int count) const;
    /**
     * 删除从指定位置到末尾的字符
     * @param startIndex 起始位置
     * @return 新字符串
     */
    LString remove(int startIndex) const;
    /**
     * 删除首次出现的子字符串
     * @param subStr 待删除子字符串
     * @return 新字符串
     */
    LString remove(const LString &subStr) const;

    /**
     * 替换首次出现的子字符串
     * @param oldSubStr 被替换子字符串
     * @param newSubStr 替换为的子字符串
     */
    void replace(const LString &oldSubStr, const LString &newSubStr);

    /**
     * 追加指定数量的字符
     * @param count 字符数量
     * @param c 要追加的字符
     */
    void append(int count, char c);

    /**
     * 判断是否为空字符串
     * @return 空返回 true
     */
    bool isEmpty() const;
};

#endif // LANSHARE_STRING_H
