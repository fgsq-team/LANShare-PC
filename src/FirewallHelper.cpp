//
// FirewallHelper.cpp
// 程序以普通权限运行（asInvoker），保证资源管理器可以向窗口拖放文件。
// 防火墙入站规则在缺失时，通过提权子进程（ShellExecute runas）自动添加，
// 仅首次运行或规则被删除时弹一次 UAC；规则已存在则静默跳过。
//

#include "FirewallHelper.h"
#include "Config.h"

#include <QProcess>
#include <QStringList>
#include <QDebug>

#if defined(PLATFORM_WINDOWS)
#include <qt_windows.h>
#endif

// 普通权限执行 netsh advfirewall firewall <args>，逐参数传递；返回退出码（0 成功）。
static int runNetsh(const QStringList &firewallArgs) {
    QProcess proc;
#if defined(PLATFORM_WINDOWS)
    proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= CREATE_NO_WINDOW;
    });
#endif
    QStringList full;
    full << QStringLiteral("advfirewall") << QStringLiteral("firewall") << firewallArgs;
    proc.start(QStringLiteral("netsh"), full);
    if (!proc.waitForStarted(3000)) {
        return -1;
    }
    proc.waitForFinished(5000);
    return proc.exitCode();
}

// 检测指定名称的防火墙规则是否已存在（普通权限即可查询）。
static bool ruleExists(const QString &ruleName) {
    return runNetsh(QStringList() << QStringLiteral("show") << QStringLiteral("rule")
                                  << (QStringLiteral("name=") + ruleName)) == 0;
}

// 普通权限下尝试添加一条规则（当前进程若是管理员则成功，否则返回 false）。
static bool addRuleNormal(const QString &ruleName, const QString &protocol, int localPort) {
    QStringList args;
    args << QStringLiteral("add") << QStringLiteral("rule")
         << (QStringLiteral("name=") + ruleName)
         << QStringLiteral("dir=in")
         << QStringLiteral("action=allow")
         << (QStringLiteral("protocol=") + protocol)
         << (QStringLiteral("localport=") + QString::number(localPort))
         << QStringLiteral("profile=any")
         << QStringLiteral("enable=yes");
    return runNetsh(args) == 0;
}

static void deleteRuleNormal(const QString &ruleName) {
    runNetsh(QStringList() << QStringLiteral("delete") << QStringLiteral("rule")
                           << (QStringLiteral("name=") + ruleName));
}

#if defined(PLATFORM_WINDOWS)
// 以提权方式（弹一次 UAC）执行一个 cmd 批处理，里面完成 delete + 两条 add。
static void elevateAddRules(int tcpPort, int udpPort,
                            const QString &tcpRule, const QString &udpRule) {
    // 用 cmd /c 串联多条 netsh，& 分隔；执行完自动关闭。
    QStringList parts;
    parts << QStringLiteral("netsh advfirewall firewall delete rule name=\"%1\"").arg(tcpRule)
          << QStringLiteral("netsh advfirewall firewall delete rule name=\"%1\"").arg(udpRule)
          << QStringLiteral("netsh advfirewall firewall add rule name=\"%1\" dir=in action=allow protocol=TCP localport=%2 profile=any enable=yes").arg(tcpRule).arg(tcpPort)
          << QStringLiteral("netsh advfirewall firewall add rule name=\"%1\" dir=in action=allow protocol=UDP localport=%2 profile=any enable=yes").arg(udpRule).arg(udpPort);
    const QString cmdLine = QStringLiteral("/c ") + parts.join(QStringLiteral(" & "));

    ShellExecuteW(nullptr,
                  L"runas",
                  L"cmd.exe",
                  reinterpret_cast<const wchar_t *>(cmdLine.utf16()),
                  nullptr,
                  SW_HIDE);
}
#endif

bool FirewallHelper::addFirewallRules() {
    const int tcpPort = config.tcpPort;
    const int udpPort = config.udpPort;
    const QString tcpRule = QStringLiteral("LANShare-TCP-%1").arg(tcpPort);
    const QString udpRule = QStringLiteral("LANShare-UDP-%1").arg(udpPort);

    // 规则已存在则直接返回，不弹 UAC、不重复添加。
    if (ruleExists(tcpRule) && ruleExists(udpRule)) {
        qDebug() << "firewall rules already exist, skip";
        return true;
    }

    // 先尝试普通权限添加（若当前以管理员运行则直接成功）。
    deleteRuleNormal(tcpRule);
    deleteRuleNormal(udpRule);
    if (addRuleNormal(tcpRule, QStringLiteral("TCP"), tcpPort) &&
        addRuleNormal(udpRule, QStringLiteral("UDP"), udpPort)) {
        qDebug() << "firewall rules added with current privileges";
        return true;
    }

    // 普通权限失败（非管理员），提权子进程添加，弹一次 UAC。
#if defined(PLATFORM_WINDOWS)
    qDebug() << "elevating to add firewall rules (UAC prompt expected)";
    elevateAddRules(tcpPort, udpPort, tcpRule, udpRule);
    // ShellExecute 是异步的，给系统一点时间完成规则写入。
    Sleep(1500);
    return ruleExists(tcpRule) && ruleExists(udpRule);
#else
    return false;
#endif
}

bool FirewallHelper::removeFirewallRules() {
    deleteRuleNormal(QStringLiteral("LANShare-TCP-%1").arg(config.tcpPort));
    deleteRuleNormal(QStringLiteral("LANShare-UDP-%1").arg(config.udpPort));
    return true;
}
