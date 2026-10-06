//
// Created by fgsqme on 2024/5/4.
//

#ifndef LANSHARE_MUTILS_H
#define LANSHARE_MUTILS_H

#define SEPARATORS "/"

#include <QString>
#include <QColor>
#include <QWidget>
#include <QFileInfo>

#include "Type.h"

/**
 * 项目工具类
 * 提供文件夹创建、文件关联、QSS 颜色解析、加密解密、文件去重等实用功能
 * @author fgsq
 * @version 1.0
 */
class mUtils {
public:
    /**
     * 递归创建多级目录
     * @param path 目录路径
     * @return 创建后的完整路径
     */
    static QString createMultipleFolders(const QString &path);

    /**
     * 设置或取消文件右键关联菜单
     * @param del true 设置关联，false 删除关联
     */
    static void setFileAssociation(bool del);

    /**
     * 从 QSS 样式表中解析指定属性颜色
     * @param styleSheet 样式表内容
     * @param className 类选择器名称
     * @param propertyName 属性名称
     * @return 解析到的颜色，未找到返回白色
     */
    static QColor parseColorFromStyleSheet(QString styleSheet, const QString &className, const QString &propertyName);

    /**
     * 判断系统是否为暗色模式
     * @return 暗色模式返回 true
     */
    static bool isDarkMode();

    /**
     * AES-256 解密消息
     * @param message 加密的消息（十六进制）
     * @param key 密钥
     * @return 解密后的明文
     */
    static QString decMessage(const QString &message, const QString &key);

    /**
     * AES-256 加密消息
     * @param message 明文消息
     * @param key 密钥
     * @return 加密后的十六进制字符串
     */
    static QByteArray encMessage(const QString &message, const QString &key);

    /**
     * 文件去重，若文件已存在则自动添加序号后缀
     * @param outFile 输出文件信息
     * @return 不重复的文件路径
     */
    static QString avoidDuplication(const QFileInfo &outFile);

    /**
     * 创建空文件（原子写入）
     * @param filename 文件路径
     */
    static void createEmptyFileWithSaveFile(const QString &filename);

    /**
     * 加密数据（异或+位移）
     * @param buffer 数据缓冲区
     * @param len 数据长度
     * @param off 偏移量
     * @param index 索引种子
     */
    static void encData(mbyte *buffer, int len, int off, mlong index);

    /**
     * 解密数据（异或+位移）
     * @param buffer 数据缓冲区
     * @param len 数据长度
     * @param off 偏移量
     * @param index 索引种子
     */
    static void decData(mbyte *buffer, int len, int off, mlong index);
};


#endif //LANSHARE_MUTILS_H
