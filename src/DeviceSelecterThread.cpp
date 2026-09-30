//
// Created by fgsq on 2023/10/9.
//

#include "DeviceSelecterThread.h"
#include "DeviceSelecter.h"

DeviceSelecterThread::DeviceSelecterThread(DeviceSelecter *deviceSelecter, LANShare *lanShare) :
        deviceSelecter(deviceSelecter), lanShare(lanShare) {
    // qDebug() << "开始";
}

void DeviceSelecterThread::run() {
    while (!deviceSelecter->isClose()) {
        if (deviceSelecter->getDeviceCount() != lanShare->getOnLineDevices().size()) {
            emit deviceSelecter->sigUpdateList();
        }
        QThread::sleep(1);
    }
}
