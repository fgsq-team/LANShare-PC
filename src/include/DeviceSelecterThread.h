//
// Created by fgsq on 2023/10/9.
//

#ifndef UNTITLED_DEVICESELECTERTHREAD_H
#define UNTITLED_DEVICESELECTERTHREAD_H


#include <QThread>

class DeviceSelecter;
class LANShare;

class DeviceSelecterThread : public QThread {
public:
    DeviceSelecterThread(DeviceSelecter *deviceSelecter, LANShare *lanShare);

protected:
    void run() override;

private:
    DeviceSelecter *deviceSelecter;
    LANShare *lanShare;
};


#endif //UNTITLED_DEVICESELECTERTHREAD_H
