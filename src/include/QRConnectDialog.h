//
// "扫码连接"二维码对话框：展示本机连接码二维码，供手机 LANShare 扫描后将本机加入设备列表。
// 二维码内容与安卓版保持一致：KEY-0-<ipv4列表,逗号分隔>-<tcp端口>。
//
// @author fgsq
// @version 1.0
#ifndef LANSHARE_QRCONNECTDIALOG_H
#define LANSHARE_QRCONNECTDIALOG_H

#include <QDialog>

class QLabel;

/**
 * 扫码连接对话框
 * 显示本机 IP 生成的二维码，手机扫描后向本机发起 FS_ADD_DEVICE 完成互加设备。
 */
class QRConnectDialog : public QDialog {
Q_OBJECT

public:
    explicit QRConnectDialog(QWidget *parent = nullptr);

private slots:
    /// 重新读取本机网卡并刷新二维码
    void refresh();

private:
    QLabel *qrLabel = nullptr;
    QLabel *ipLabel = nullptr;
};

#endif //LANSHARE_QRCONNECTDIALOG_H
