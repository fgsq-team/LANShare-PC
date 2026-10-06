//
// Created by fgsq on 2023/10/9.
//

#include <QLabel>
#include <QVBoxLayout>
#include "DeviceItem.h"

DeviceItem::DeviceItem(QWidget *parent) : QWidget(parent) {
    nameLabel = new QLabel();
    ipLabel = new QLabel();
    iconLabel = new QLabel();
    batteryLabel = new QLabel();
    batteryTextLabel = new QLabel();
    batteryLabel->setFixedSize(15, height());
    batteryTextLabel->setFixedSize(35, height());
    QFont font = batteryTextLabel->font();
    font.setPointSize(8);
    batteryTextLabel->setFont(font);

    auto *mainLayout = new QHBoxLayout(this);
    auto *textLayout = new QVBoxLayout();
    // 设置图标
    textLayout->addWidget(nameLabel);
    textLayout->addWidget(ipLabel);
    mainLayout->addWidget(iconLabel);
    mainLayout->addLayout(textLayout);
    mainLayout->addWidget(batteryLabel);
    mainLayout->addWidget(batteryTextLabel);
    setLayout(mainLayout);
}

const Device &DeviceItem::getDevice() const {
    return device;
}

void DeviceItem::setDevice(const Device &device) {
    DeviceItem::device = device;
    nameLabel->setText(device.getDevName() + " (V" + QString::number(device.getDataVersion()) + ")");
    if (device.getDevIp().isEmpty()) {
        ipLabel->hide();
    } else {
        ipLabel->setText(device.getDevIp());
    }
    batteryTextLabel->setText(QString::number(device.getBatteryLevel()) + "%");
    mbyte chargeStatus = device.getChargeStatus();
    if (device.getDevMode() == Device::L_ANDROID) {
        QPixmap pixmap(":/img/ic_phone.png");
        iconLabel->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (device.getDevMode() == Device::L_WIN) {
        QPixmap pixmap(":/img/ic_win.png");
        iconLabel->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        QPixmap pixmap(":/img/ic_launcher_32.png");
        iconLabel->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    if (device.getBatteryLevel() > 0) {
        batteryLabel->setVisible(true);
        batteryTextLabel->setVisible(true);
        QPixmap pixmap;
        if (chargeStatus == 1) {
            pixmap = {":/img/ic_charging.png"};
            batteryTextLabel->setStyleSheet("QLabel { color : #00bd0d; }");
        } else {
            int batteryLevel = device.getBatteryLevel();
            if (batteryLevel > 80 && batteryLevel <= 100) {
                pixmap = {":/img/ic_battery4.png"};
                batteryTextLabel->setStyleSheet("QLabel { color : #00bd0d; }");
            } else if (batteryLevel > 60) {
                pixmap = {":/img/ic_battery3.png"};
                batteryTextLabel->setStyleSheet("QLabel { color : #00bd0d; }");
            } else if (batteryLevel > 40) {
                pixmap = {":/img/ic_battery2.png"};
                batteryTextLabel->setStyleSheet("QLabel { color : #65a30d; }");
            } else if (batteryLevel > 10) {
                pixmap = {":/img/ic_battery1.png"};
                batteryTextLabel->setStyleSheet("QLabel { color : #eab308; }");
            } else if (batteryLevel > 0) {
                pixmap = {":/img/ic_battery0.png"};
                batteryTextLabel->setStyleSheet("QLabel { color : #dc2626; }");
            } else {
                pixmap = {":/img/ic_null.png"};
                batteryTextLabel->setStyleSheet("QLabel { color : #00000000; }");
            }
        }
        batteryLabel->setPixmap(pixmap.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        batteryLabel->setHidden(true);
        batteryTextLabel->setHidden(true);
    }

    batteryLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    batteryTextLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    //    update();
}

DeviceItem::~DeviceItem() {
    delete nameLabel;
    delete ipLabel;
    delete iconLabel;
}
