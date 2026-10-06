#include "LANShareWindow.h"
#include "LANShare.h"
#include "Config.hpp"
#include "DataEnc.h"
#include "CleanupTool.h"
#include "SignalHandler.h"
#include "TranslationManager.h"
#include <QApplication>
#include <thread>
#include <QLocalSocket>
#include <QLocalServer>
#include <QCoreApplication>
#include <QImageReader>
#include <cstdio>
void installTranslator();

bool OPEN_DEBUG = true;
const QString serverName = "lanshare_server";

void readSocketData(QLocalSocket *socket, LANShareWindow *w) {
    QByteArray data = socket->readAll();
    qDebug() << "Received data size:" << data.size();
    const char *charData = data.constData();
    DataDec dataDec((mbyte *) charData, data.size());
    std::vector<LFile *> list;
    for (int i = 0; i < dataDec.getCount(); ++i) {
        char *path = dataDec.getStr();
        qDebug() << "path:" << path;
        QFileInfo fileInfo(path); //绝对路径与相对路径都可以
        if (fileInfo.exists()) {
            auto *lFile = new LFile();
            lFile->setFileName(fileInfo.fileName());
            lFile->setFileSize(fileInfo.size());
            lFile->setPath(fileInfo.filePath());
            lFile->setIsDirectory(fileInfo.isDir());
            list.push_back(lFile);
        }
        delete[] path;
    }
    w->setWindowToTopLayer();
    if (!list.empty()) {
        w->showDeviceSelecter(list);
    }
}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    Config::instance().init();
    // 初始化翻译管理器
    TranslationManager::instance()->loadLanguageFromSettings();
    QFont f("幼圆", 9);
    QApplication::setFont(f);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::addLibraryPath("./plugins");
    QLocalSocket socket;
    socket.connectToServer(serverName);

    if (socket.waitForConnected(1000))//创建失败，说明已经有一个程序运行，
    {
        auto buff = new mbyte[1024 * 1024 + DataEnc::headerSize()];
        DataEnc dataEnc(buff, 1024 * 1024 + DataEnc::headerSize());
        for (int i = 1; i < argc; ++i) {
            QString path = QString::fromLocal8Bit(argv[i]);
            dataEnc.putString(path);
        }
        dataEnc.setCmd(LOCAL_PARAMS);
        dataEnc.setCount(argc - 1);
        qDebug() << "getDataLen:" << dataEnc.getDataLen();
        QByteArray qByteArray;
        qByteArray.append((char *) dataEnc.getData(), dataEnc.getDataLen());
        qDebug() << "QByteArray size:" << qByteArray.size();
        if (socket.write(qByteArray) == -1) {
            qDebug() << "Error writing data:" << socket.errorString();
        }
        socket.flush();
        socket.waitForBytesWritten();
        socket.close();
        delete[] buff;
        qApp->quit();
        return -1;
    }
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    if (!server.listen(serverName)) {
        qDebug() << "Unable to start the server:" << server.errorString();
        return 1;
    }
    LANShareWindow w;
    LANShare lanShare(&w);
    // TCP 文件接收线程
    std::thread tTcpServer([&lanShare]() { lanShare.tcpProtocol.createTcpServer(); });
    tTcpServer.detach();
    // UDP 接收命令线程
    std::thread tRunRecive([&lanShare]() { lanShare.udpProtocol.handleUdp(); });
    tRunRecive.detach();
    // 扫描设备线程
    std::thread tScannDevice([&lanShare]() { lanShare.deviceManager.scannDevice(); });
    tScannDevice.detach();
    w.setWindowTitle("LANShare");
    w.show();

    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket *socket = server.nextPendingConnection();
        if (socket) {
            qDebug() << "New connection established";
            if (socket->waitForReadyRead()) {
                readSocketData(socket, &w);
            }
            // 通信完成后，关闭连接
            socket->close();
            socket->deleteLater();
        }
    });
    // 设置退出信号的处理
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [&server, &lanShare]() {
        qDebug() << "About to quit, closing server.";
        lanShare.close();
        server.close();
    });

    if (argc > 1) {
        std::vector<LFile *> list;
        for (int i = 1; i < argc; ++i) {
            QString path = QString::fromLocal8Bit(argv[i]);
            qDebug() << "Send File:" << path;
            QFileInfo fileInfo(path); //绝对路径与相对路径都可以
            if (fileInfo.exists()) {
                auto *lFile = new LFile();
                lFile->setFileName(fileInfo.fileName());
                lFile->setFileSize(fileInfo.size());
                lFile->setPath(fileInfo.filePath());
                lFile->setIsDirectory(fileInfo.isDir());
                list.push_back(lFile);
            }
        }
        qDebug() << "Send File Count:" << list.size();
        if (!list.empty()) {
            w.setWindowToTopLayer();
            w.showDeviceSelecter(list);
        }
    }

    return QApplication::exec();
}