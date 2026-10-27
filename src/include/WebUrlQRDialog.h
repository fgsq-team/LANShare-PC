//
// "网页二维码"对话框：展示本机网页地址（http://ip:port）的二维码，
// 供手机相机/浏览器直接扫码访问 PC 内置网页版 LANShare。
// 与"扫码连接"不同，此处内容就是普通 URL，任意扫码工具都能识别。
//
// @author fgsq
// @version 1.0
#ifndef LANSHARE_WEBURLQRDIALOG_H
#define LANSHARE_WEBURLQRDIALOG_H

#include <QDialog>

class QLabel;
class QComboBox;

/**
 * 网页地址二维码对话框
 * 列出本机所有 IPv4 网卡地址，选中的地址生成对应网页 URL 二维码。
 */
class WebUrlQRDialog : public QDialog {
Q_OBJECT

public:
    explicit WebUrlQRDialog(QWidget *parent = nullptr);

private slots:
    /// 重新读取本机网卡并刷新 IP 下拉框
    void refresh();
    /// 依据当前选中的 IP 重新生成二维码
    void updateQr();

private:
    QComboBox *ipCombo = nullptr;
    QLabel *qrLabel = nullptr;
    QLabel *urlLabel = nullptr;
};

#endif //LANSHARE_WEBURLQRDIALOG_H
