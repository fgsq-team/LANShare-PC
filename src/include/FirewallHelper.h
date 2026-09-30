//
// FirewallHelper.h
// 程序启动时自动添加 Windows 防火墙入站放行规则（TCP 文件传输 / UDP 设备发现）。
// 规则写入 Windows 系统防火墙，不是写入 exe；需要管理员权限。
//

#ifndef LANSHARE_FIREWALLHELPER_H
#define LANSHARE_FIREWALLHELPER_H

class FirewallHelper {
public:
    // 添加入站规则：TCP 文件端口、UDP 发现端口，所有网络配置文件生效。
    // 重复执行是幂等的（先删后加）。返回 true 表示全部成功。
    static bool addFirewallRules();

    // 删除上述规则（清理/卸载时使用）。
    static bool removeFirewallRules();
};

#endif //LANSHARE_FIREWALLHELPER_H
