//
// Created by fgsq on 2026/10/12.
//

#include "SyncFeatureHandler.h"

#include "Config.hpp"
#include "CustomDataStream.h"
#include "Device.h"
#include "LANShareWindow.h"
#include "mUtils.h"

void SyncFeatureHandler::handleMessage(const Device &device, CustomDataStream *stream) {
    bool isClip = stream->readBoolean();
    QString messageEnc = QString::fromUtf8(stream->readString().c_str());
    try {
        const QString message = mUtils::decMessage(messageEnc, Config::instance().messageKey);
        qDebug() << "devName:" << device.getDevName() << " message:" << message;
        emit
        LANShareWindow::getInstance()->sigNewMessage(device, message, true);
        if (isClip) {
            emit
            LANShareWindow::getInstance()->sigCopyText(message);
        }
        qDebug("client close");
    } catch (const std::exception &e) {
        qDebug() << "message dec error" << e.what();
    }
    stream->close();
}

// 暂时不支持媒体同步
void SyncFeatureHandler::handleMediaSync(const Device &device, CustomDataStream *stream) {
    (void) device;
    stream->close();
}
