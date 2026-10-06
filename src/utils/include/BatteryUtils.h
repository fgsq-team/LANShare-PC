//
// Created by user on 2025/1/14.
//

#ifndef BATTERYUTILS_H
#define BATTERYUTILS_H
#include <qstring.h>


/**
 * 电池工具类
 * 提供跨平台的电池电量和充电状态查询功能
 * @author fgsq
 * @version 1.0
 */
class BatteryUtils {
public:
    /**
     * 获取电池电量百分比
     * @return 电量百分比（0-100），失败返回 -1
     */
    static int getBatteryPercentage();

    /**
     * 获取电池充电状态
     * @return 1 充电中/已连接电源，0 放电中，-1 未知
     */
    static int getBatteryStatus();
};


#endif //BATTERYUTILS_H
