//
// Created by fgsqme on 2021/9/26 0026.
//

#ifndef LANSHARE_UPDATECHECKER_H
#define LANSHARE_UPDATECHECKER_H

#include <QObject>

class QWidget;
class QNetworkAccessManager;

/**
 * 版本更新检测工具类
 * 封装"检查更新"的网络请求与结果处理，供主窗口启动时自动检测与"关于"页手动检测复用。
 * 仅暴露静态方法，不创建实例；内部使用进程级共享的 QNetworkAccessManager（避免
 * Qt6 下反复创建/销毁 manager 触发内部线程/连接池未收敛导致的堆损坏崩溃），
 * 每个 reply 通过 deleteLater 自清理。继承 QObject 仅为提供 tr() 翻译上下文。
 * @author fgsq
 * @version 1.0
 */
class UpdateChecker : public QObject {
Q_OBJECT

public:
    /**
     * 发起一次更新检测
     * @param parent 弹窗父窗口（请求返回前若被销毁则自动跳过弹窗）
     * @param manual 是否为用户手动触发：
     *               - true ：无视"跳过版本"记录，且无新版本或失败时给出提示
     *               - false：启动时自动检测，遵循跳过版本逻辑，无更新时静默
     */
    static void check(QWidget *parent, bool manual);

private:
    /** 进程级共享的 QNetworkAccessManager，与 QApplication 生命周期一致 */
    static QNetworkAccessManager &sharedManager();
};

#endif //LANSHARE_UPDATECHECKER_H
