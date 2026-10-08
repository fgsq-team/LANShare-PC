//
// Created by fgsqme on 2021/9/26 0026.
//

#include "UpdateChecker.h"
#include "Config.hpp"

#include <QPointer>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QDesktopServices>
#include <QIcon>
#include <QSettings>
#include <QUrl>
#include <QDebug>
#include <QApplication>

namespace {

/** 解析版本响应并按自动/手动模式给出不同提示 */
void handleReply(QNetworkReply *reply, const QPointer<QWidget> &guard, bool manual) {
    if (reply->error() == QNetworkReply::NoError) {
        // 读取响应数据
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject object = doc.object();
        int versionCode = object["versionCode"].toInt();
        bool isForceUpdate = object["isForceUpdate"].toBool();
        int forceUpdateMinVersion = object["forceUpdateMinVersion"].toInt();
        QString versionName = object["version"].toString();
        QString fileUrl = object["fileUrl"].toString();
        QString updateContent = object["updateContent"].toString();
        QSettings *settings = Config::instance().getSettings();
        int forceVersion = settings->value(FORCE_VERSION, -1).toInt();
        isForceUpdate = isForceUpdate && LANSHARE_VERSION < forceUpdateMinVersion;
        // 手动检测忽略"跳过版本"记录；自动检测遵循该记录
        bool hasUpdate = (versionCode > LANSHARE_VERSION && (manual || versionCode > forceVersion)) || isForceUpdate;
        if (hasUpdate && guard) {
            QMessageBox msgBox(guard.data());
            msgBox.setText(updateContent);
            msgBox.setWindowTitle(UpdateChecker::tr("有新版本更新: ") + versionName);
            msgBox.setWindowIcon(QIcon(":/img/ic_launcher.png"));
            msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
            msgBox.setDefaultButton(QMessageBox::No);
            msgBox.setButtonText(QMessageBox::No,
                                 isForceUpdate ? UpdateChecker::tr("退出") : UpdateChecker::tr("不更新"));
            msgBox.setButtonText(QMessageBox::Yes, UpdateChecker::tr("打开下载链接"));
            // 显示消息框，并获取用户的选择
            int ret = msgBox.exec();
            // 根据用户的选择进行相应的操作
            if (ret == QMessageBox::Yes) {
                if (!fileUrl.startsWith("http://")) {
                    fileUrl = LANSHARE_SERVER + fileUrl;
                }
                QDesktopServices::openUrl(fileUrl);
            } else {
                if (isForceUpdate) {
                    QApplication::quit();
                    return;
                }
                settings->setValue(FORCE_VERSION, versionCode);
            }
        } else if (manual && guard) {
            QMessageBox::information(guard.data(), UpdateChecker::tr("检查更新"), UpdateChecker::tr("当前已是最新版本"));
        }
    } else {
        // 请求失败，输出错误信息
        qDebug() << "Error:" << reply->errorString();
        if (manual && guard) {
            QMessageBox::warning(guard.data(), UpdateChecker::tr("检查更新"),
                                 UpdateChecker::tr("检测更新失败：") + reply->errorString());
        }
    }
}

} // namespace

/** 进程级共享的 QNetworkAccessManager：函数作用域静态，与 QApplication 同生命周期 */
QNetworkAccessManager &UpdateChecker::sharedManager() {
    static QNetworkAccessManager inst;
    return inst;
}

/**
 * 发起一次更新检测
 * 用 QPointer 守护父窗口，请求返回前若窗口已销毁则跳过弹窗；
 * 只销毁 reply，共享的 manager 不销毁，避免 Qt6 下频繁创建/销毁 manager 引发的堆损坏崩溃。
 */
void UpdateChecker::check(QWidget *parent, bool manual) {
    QNetworkRequest request(QUrl(
            QString(LANSHARE_SERVER)
            + "/get_version"
            + "?type=" + APP_TYPE
            + "&versionCode=" + QString::number(LANSHARE_VERSION)
            + "&versionName=" + LANSHARE_VERSION_NAME
    ));
    QNetworkReply *reply = sharedManager().get(request);
    const QPointer<QWidget> guard(parent);
    // 以 reply 作为上下文：reply 销毁时连接自动断开；只 deleteLater reply
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, guard, manual]() {
        handleReply(reply, guard, manual);
        reply->deleteLater();
    });
}
