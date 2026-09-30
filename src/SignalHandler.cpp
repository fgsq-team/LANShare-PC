//
// Created by fgsqme on 2024/5/14.
//

#include "SignalHandler.h"

QSharedMemory *SignalHandler::sharedMemory = nullptr;
QLocalServer *SignalHandler::localServer = nullptr;

SignalHandler::SignalHandler(QSharedMemory *sharedMemory, QLocalServer *localServer, QObject *parent)
        : QObject(parent) {
    SignalHandler::sharedMemory = sharedMemory;
    SignalHandler::localServer = localServer;
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGABRT, handleSignal);
    std::signal(SIGSEGV, handleSignal);
#if defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    std::signal(SIGPIPE, handleSignal);
    std::signal(SIGKILL, handleSignal);
#endif
}

void SignalHandler::handleSignal(int signal) {
    qDebug() << "handleSignal";
    if (sharedMemory && sharedMemory->isAttached()) {
        sharedMemory->detach();
    }
    if (localServer && localServer->isListening()) {
        localServer->close();
    }
    std::exit(signal);
}
