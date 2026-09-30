//
// Created by fgsqme on 2024/5/4.
//

#include "mUtils.h"
#include "Type.h"
#include "qaesencryption.h"
#include "Config.h"
#include <QString>
#include <QDir>
#include <QWidget>
#include <QCoreApplication>
#include <qrandom.h>
#include <QSettings>
#include <QRegularExpression>

#if defined(PLATFORM_WINDOWS)

#include <windows.h>

#elif defined(PLATFORM_MACOS)
#include <QOperatingSystemVersion>
#include <objc/objc-runtime.h>
#endif

QString mUtils::decMessage(const QString &message,const QString& key) {
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::ECB, QAESEncryption::PKCS7);
    QByteArray input(message.toUtf8());
    QByteArray keyByteArray = key.toUtf8();
    return QAESEncryption::RemovePadding(encryption.decode(QByteArray::fromHex(input), keyByteArray), QAESEncryption::PKCS7);
}


QByteArray mUtils::encMessage(const QString &message,const QString& key) {
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::ECB, QAESEncryption::PKCS7);
    QByteArray input(message.toUtf8());
    QByteArray keyByteArray = key.toUtf8();
    return encryption.encode(input, keyByteArray).toHex();
}


bool mUtils::isDarkMode() {
    bool isDark = false;

#if defined(PLATFORM_WINDOWS)
    DWORD value = 0;
    DWORD bufferSize = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
                     L"AppsUseLightTheme", RRF_RT_DWORD, nullptr, &value, &bufferSize) == ERROR_SUCCESS) {
        isDark = (value == 0);
    }
#elif defined(PLATFORM_MACOS)
    // 获取应用程序的调色板
    QPalette palette = QApplication::palette();
    // 判断调色板的背景颜色是否是暗色
    QColor backgroundColor = palette.color(QPalette::Window);
    int brightness = (backgroundColor.red() * 299 + backgroundColor.green() * 587 + backgroundColor.blue() * 114) / 1000;
    return brightness < 128;
#elif defined(PLATFORM_LINUX)
#endif
    return isDark;
}

QColor mUtils::parseColorFromStyleSheet(QString styleSheet, const QString &className, const QString &propertyName) {
    QString pattern = QString(R"(%1\s*\{[^}]*%2\s*:\s*([^;]+);)").arg(className, propertyName);
    QRegularExpression regex(pattern);
    QRegularExpressionMatch match = regex.match(styleSheet);
    if (match.hasMatch()) {
        QString colorString = match.captured(1).trimmed();
        return QColor(colorString);
    }

    return Qt::white;
}

QString mUtils::createMultipleFolders(const QString &path) {
    QDir dir(path);
    if (dir.exists(path)) {
        return path;
    }
    QString parentDir = createMultipleFolders(path.mid(0, path.lastIndexOf(SEPARATORS)));
    QString dirName = path.mid(path.lastIndexOf(SEPARATORS) + 1);
    QDir parentPath(parentDir);
    if (!dirName.isEmpty()) {
        if (parentPath.mkpath(dirName)) {
            qDebug() << "创建文件夹" << parentPath.path() << "成功";
        } else {
            qDebug() << "创建文件夹" << parentPath.path() << "失败";
        }
    }
    return parentDir + SEPARATORS + dirName;
}

void mUtils::setFileAssociation(bool del) {
    if (del) {
#if defined(PLATFORM_WINDOWS)
        QString executablePath = QCoreApplication::applicationFilePath();
        QString regPath = R"(HKEY_CURRENT_USER\Software\Classes\*\shell\OpenWithLANShare)";
        QSettings settings(regPath, QSettings::NativeFormat);
//        QString value = settings.value("command/default", "").toString();
        settings.setValue("icon", QDir::toNativeSeparators(executablePath));
        settings.setValue("MUIVerb", "通过LANShare发送");
        settings.beginGroup("command");
        settings.setValue(".", QDir::toNativeSeparators(executablePath + " \"%1\""));
        settings.endGroup();
        settings.setValue("Position", 0); // 0 表示最高优先级
        QString regPathDir = R"(HKEY_CLASSES_ROOT\Directory\shell\OpenWithLANShare)";
        QSettings settingsDir(regPathDir, QSettings::NativeFormat);
        settingsDir.setValue("MUIVerb", "通过LANShare发送");
        settingsDir.setValue("icon", QDir::toNativeSeparators(executablePath));
        settingsDir.beginGroup("command");
        settingsDir.setValue(".", QDir::toNativeSeparators(executablePath + " \"%1\""));
        settingsDir.endGroup();
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#endif
    } else {
#if defined(PLATFORM_WINDOWS)
        // 删除HKEY_CURRENT_USER\Software\Classes\*\shell\OpenWithLANShare
        QString regPathToDelete = R"(HKEY_CURRENT_USER\Software\Classes\*\shell\OpenWithLANShare)";
        QSettings deleteSettings(regPathToDelete, QSettings::NativeFormat);
        deleteSettings.clear(); // 清除所有项和组，相当于删除整个注册表项
        // 删除HKEY_CLASSES_ROOT\Directory\shell\OpenWithLANShare
        QString regPathDirToDelete = R"(HKEY_CLASSES_ROOT\Directory\shell\OpenWithLANShare)";
        QSettings deleteSettingsDir(regPathDirToDelete, QSettings::NativeFormat);
        deleteSettingsDir.clear(); // 清除所有项和组，相当于删除整个注册表项
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#endif
    }
}

QString mUtils::generateRandomString(int length) {
    const QString characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString result;
    result.reserve(length);
    for (int i = 0; i < length; i++) {
        int index = QRandomGenerator::global()->bounded(characters.length());
        result.append(characters.at(index));
    }
    return result;
}
