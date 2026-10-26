//
// "扫码连接"二维码对话框实现。
//
#include "QRConnectDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStringList>
#include <vector>

#include "QrCode.hpp"
#include "Config.hpp"
#include "LANShare.h"
#include "Device.h"

QRConnectDialog::QRConnectDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("扫码连接"));
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    auto *tip = new QLabel(tr("用手机 LANShare 的“扫码连接”扫描下方二维码，即可把本机添加到手机设备列表。"));
    tip->setWordWrap(true);
    tip->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    qrLabel = new QLabel();
    qrLabel->setAlignment(Qt::AlignCenter);
    qrLabel->setMinimumSize(240, 240);

    ipLabel = new QLabel();
    ipLabel->setAlignment(Qt::AlignCenter);
    ipLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *refreshBtn = new QPushButton(tr("刷新"));
    auto *closeBtn = new QPushButton(tr("关闭"));

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(closeBtn);
    btnRow->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->addWidget(tip);
    layout->addWidget(qrLabel);
    layout->addWidget(ipLabel);
    layout->addLayout(btnRow);

    connect(refreshBtn, &QPushButton::clicked, this, &QRConnectDialog::refresh);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    refresh();
    adjustSize();
}

void QRConnectDialog::refresh() {
    QStringList ips;
    if (LANShare::getInstance() != nullptr) {
        const std::vector<Device> mDevices = LANShare::getInstance()->getMDevices();
        for (const Device &d: mDevices) {
            if (!d.getDevIp().isEmpty() && !ips.contains(d.getDevIp())) {
                ips << d.getDevIp();
            }
        }
    }

    if (ips.isEmpty()) {
        qrLabel->setText(tr("未获取到本机 IP"));
        ipLabel->setText("");
        return;
    }

    // 与安卓版 DeviceQrCodeActivity 完全一致的编码格式，确保手机能识别
    const QString content = QString(KEY) + "-0-" + ips.join(",") +
                            "-" + QString::number(Config::instance().tcpPort);
    const QImage img = lanqrcode::makeImage(content, 6, 4);
    if (img.isNull()) {
        qrLabel->setText(tr("二维码生成失败"));
    } else {
        qrLabel->setPixmap(QPixmap::fromImage(img));
    }
    ipLabel->setText(tr("本机地址：%1").arg(ips.join(", ") + ":" + QString::number(Config::instance().tcpPort)));
}
