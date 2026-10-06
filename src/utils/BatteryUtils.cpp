//
// Created by user on 2025/1/14.
//

#include "BatteryUtils.h"
#include "Type.h"
#if defined(PLATFORM_WINDOWS)
#include <windows.h>
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#include <fstream>
#include <string>
#include <QDir>
#endif
#if defined(PLATFORM_LINUX)
// 获取电池设备路径
QString getBatteryPath() {
    QDir powerSupplyDir("/sys/class/power_supply");
    if (!powerSupplyDir.exists()) {
        // qWarning() << "Power supply directory does not exist!";
        return {};
    }
    // 遍历目录中的所有子目录
    QStringList entries = powerSupplyDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QString fullPath = powerSupplyDir.absoluteFilePath(entry);
        QFile typeFile(fullPath + "/type");
        // 检查是否为 "Battery" 类型的设备
        if (typeFile.exists() && typeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString type = typeFile.readAll().trimmed();
            if (type == "Battery") {
                return fullPath; // 返回电池设备路径
            }
        }
    }
    // qWarning() << "No battery device found!";
    return {};
}

// 读取文件内容的工具函数
QString readFileContent(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return file.readAll().trimmed();
    }
    return {};
}

#endif

#if defined(PLATFORM_MACOS)
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#endif


int BatteryUtils::getBatteryPercentage()
{
#if defined(PLATFORM_WINDOWS)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        if (sps.BatteryLifePercent == 255) {
            return -1;
        }
        return sps.BatteryLifePercent;
    } else {
        return -1;
    }
#elif defined(PLATFORM_LINUX)
    QString batteryPath = getBatteryPath();
    if (batteryPath.isEmpty()) {
        // qWarning() << "No battery found on this system.";
        return -1; // 表示没有找到电池
    }
    // 获取电量百分比
    QString capacityFile = batteryPath + "/capacity";
    QString capacityStr = readFileContent(capacityFile);
    bool ok = false;
    int capacity = capacityStr.toInt(&ok);
    if (ok) {
        return capacity; // 返回电量百分比
    } else {
        // qWarning() << "Failed to parse battery capacity.";
        return -1; // 表示解析失败
    }
#elif defined(PLATFORM_MACOS)
#elif defined(PLATFORM_MACOS)
    CFTypeRef powerInfo = IOPSCopyPowerSourcesInfo();
    if (!powerInfo) {
        return -1;
    }

    CFArrayRef powerSources = IOPSCopyPowerSourcesList(powerInfo);
    if (!powerSources) {
        CFRelease(powerInfo);
        return -1;
    }

    int percentage = -1;
    CFIndex count = CFArrayGetCount(powerSources);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef source = (CFDictionaryRef)CFArrayGetValueAtIndex(powerSources, i);
        CFStringRef name = (CFStringRef)CFDictionaryGetValue(source, CFSTR(kIOPSNameKey));
        if (name) {
            CFNumberRef capacity = (CFNumberRef)CFDictionaryGetValue(source, CFSTR(kIOPSCurrentCapacityKey));
            CFNumberRef maxCapacity = (CFNumberRef)CFDictionaryGetValue(source, CFSTR(kIOPSMaxCapacityKey));

            if (capacity && maxCapacity) {
                int currentCap, maxCap;
                CFNumberGetValue(capacity, kCFNumberIntType, &currentCap);
                CFNumberGetValue(maxCapacity, kCFNumberIntType, &maxCap);

                if (maxCap > 0) {
                    percentage = (currentCap * 100) / maxCap;
                    break;
                }
            }
        }
    }
    CFRelease(powerSources);
    CFRelease(powerInfo);
    return percentage;
#else
    return -1; // 不支持的平台
#endif
}

int BatteryUtils::getBatteryStatus()
{
#if defined(PLATFORM_WINDOWS)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        return (sps.ACLineStatus == 1) ? 1 : 0;
    } else {
        return -1;
    }
#elif defined(PLATFORM_LINUX)
    std::ifstream statusFile("/sys/class/power_supply/BAT0/status");
    if (!statusFile.is_open())
    {
        // qDebug() << "Failed to open battery status file.";
        return -1;
    }
    std::string status;
    std::getline(statusFile, status);
    statusFile.close();
    if (status == "Charging")
    {
        return 1;
    }
    else if (status == "Discharging")
    {
        return 0;
    }
    else if (status == "Full")
    {
        return 1;
    }
    else
    {
        return -1;
    }
#elif defined(PLATFORM_MACOS)
    CFTypeRef powerInfo = IOPSCopyPowerSourcesInfo();
    if (!powerInfo) {
        return -1;
    }
    CFArrayRef powerSources = IOPSCopyPowerSourcesList(powerInfo);
    if (!powerSources) {
        CFRelease(powerInfo);
        return -1;
    }
    int status = -1;
    CFIndex count = CFArrayGetCount(powerSources);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef source = (CFDictionaryRef)CFArrayGetValueAtIndex(powerSources, i);
        CFStringRef name = (CFStringRef)CFDictionaryGetValue(source, CFSTR(kIOPSNameKey));
        if (name) {
            CFStringRef powerState = (CFStringRef)CFDictionaryGetValue(source, CFSTR(kIOPSPowerSourceStateKey));
            if (powerState) {
                if (CFStringCompare(powerState, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo) {
                    status = 1; // 充电中或已充满
                    break;
                } else if (CFStringCompare(powerState, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo) {
                    status = 0; // 使用电池供电
                    break;
                }
            }
        }
    }
    CFRelease(powerSources);
    CFRelease(powerInfo);
    return status;
#else
        return -1;
#endif
}
