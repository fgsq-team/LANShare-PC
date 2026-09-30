//
// Created by fgsq on 2023/10/9.
//

#ifndef UNTITLED_DEVICEITEM_H
#define UNTITLED_DEVICEITEM_H

#include <QWidget>
#include <QLabel>
#include "Device.h"

class DeviceItem : public QWidget {
Q_OBJECT
public:
    explicit DeviceItem(QWidget *parent = nullptr);
    ~DeviceItem() override;
    const Device &getDevice() const;

    void setDevice(const Device &device);

private:
    Device device;
    QLabel *nameLabel;
    QLabel *ipLabel;
    QLabel *iconLabel;
    QLabel *batteryLabel;
    QLabel *batteryTextLabel;
};


#endif //UNTITLED_DEVICEITEM_H
