//
// "网页二维码"对话框实现。
//
#include "WebUrlQRDialog.h"

#include <QComboBox>
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

WebUrlQRDialog::WebUrlQRDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("网页二维码"));
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);

    auto *tip = new QLabel(tr("用手机相机或浏览器扫描下方网页二维码，即可打开电脑上的 LANShare 网页版。"));
    tip->setWordWrap(true);
    tip->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    ipCombo = new QComboBox();
    ipCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    qrLabel = new QLabel();
    qrLabel->setAlignment(Qt::AlignCenter);
    qrLabel->setMinimumSize(240, 240);

    urlLabel = new QLabel();
    urlLabel->setAlignment(Qt::AlignCenter);
    urlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *refreshBtn = new QPushButton(tr("刷新"));
    auto *closeBtn = new QPushButton(tr("关闭"));

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(closeBtn);
    btnRow->addStretch();

    auto *ipRow = new QHBoxLayout();
    ipRow->addStretch();
    ipRow->addWidget(new QLabel(tr("网卡：")));
    ipRow->addWidget(ipCombo);
    ipRow->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->addWidget(tip);
    layout->addLayout(ipRow);
    layout->addWidget(qrLabel);
    layout->addWidget(urlLabel);
    layout->addLayout(btnRow);

    connect(refreshBtn, &QPushButton::clicked, this, &WebUrlQRDialog::refresh);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(ipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WebUrlQRDialog::updateQr);

    refresh();
    adjustSize();
}

void WebUrlQRDialog::refresh() {
    QStringList ips;
    if (LANShare::getInstance() != nullptr) {
        const std::vector<Device> mDevices = LANShare::getInstance()->getMDevices();
        for (const Device &d: mDevices) {
            if (!d.getDevIp().isEmpty() && !ips.contains(d.getDevIp())) {
                ips << d.getDevIp();
            }
        }
    }

    // 保持当前选中项（若新列表中仍存在）避免刷新后跳回第一项
    const QString prev = ipCombo->currentText();
    ipCombo->blockSignals(true);
    ipCombo->clear();
    if (!ips.isEmpty()) {
        ipCombo->addItems(ips);
        const int idx = ips.indexOf(prev);
        if (idx >= 0) {
            ipCombo->setCurrentIndex(idx);
        }
    }
    ipCombo->blockSignals(false);

    updateQr();
}

void WebUrlQRDialog::updateQr() {
    const QString ip = ipCombo->currentText();
    if (ip.isEmpty()) {
        qrLabel->setText(tr("未获取到本机 IP"));
        urlLabel->setText("");
        return;
    }

    // 与主界面 web_address 展示的地址保持一致：http://<ip>:<tcp端口>
    const QString url = "http://" + ip + ":" + QString::number(Config::instance().tcpPort);
    const QImage img = lanqrcode::makeImage(url, 6, 4);
    if (img.isNull()) {
        qrLabel->setText(tr("二维码生成失败"));
    } else {
        qrLabel->setPixmap(QPixmap::fromImage(img));
    }

    if (!Config::instance().webService) {
        urlLabel->setText(url + tr("（请先在主页勾选“开启网页功能”）"));
    } else {
        urlLabel->setText(url);
    }
}
