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
    QString batteryPath = getBatteryPath();
    if (batteryPath.isEmpty()) {
        return -1; // 表示没有找到电池
    }
    QString capacityFile = batteryPath + "/status";
    std::ifstream statusFile(capacityFile.toStdString());
    if (!statusFile.is_open())
    {
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
#else
        return -1;
#endif
}
