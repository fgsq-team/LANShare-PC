//
// Created by fgsqme on 2024/5/14.
//

#ifndef LANSHARE_SIGNALHANDLER_H
#define LANSHARE_SIGNALHANDLER_H

#include <QObject>
#include <QSharedMemory>
#include <csignal>
#include <QLocalServer>
#include "Type.h"

class SignalHandler : public QObject {
Q_OBJECT

public:
    SignalHandler(QSharedMemory *sharedMemory, QLocalServer *localSocket, QObject *parent = nullptr);

    static void handleSignal(int signal);

private:
    static QSharedMemory *sharedMemory;
    static QLocalServer *localServer;
};

#endif //LANSHARE_SIGNALHANDLER_H
