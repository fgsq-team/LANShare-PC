//
// Created by fgsqme on 2022/7/24.
//

#ifndef LANSHARE_WIN_CONFIG_H
#define LANSHARE_WIN_CONFIG_H

#include "Type.h"
#include "qaesencryption.h"
#include <QUuid>


// 局域网通讯命令
#define UDP_GET_DEVICES  1001            // 获取设备
#define UDP_SET_DEVICES  1002            // 设置设备
#define UDP_DEVICE_OFF_LINE 1003         // 设备下线
#define UDP_MESSAGE  1004                // 广播消息
#define UDP_MESSAGE_TO_CLIPBOARD  1005   // 广播消息到剪切板
#define UDP_SEND_MEDIA_MUTE  1006        // (媒体) 静音
#define UDP_SEND_MEDIA_RESTORE  1007     // (媒体) 恢复音量
#define UDP_SEND_MEDIA_PAUSE  1008       // (媒体) 暂停
#define UDP_SEND_MEDIA_NEXT  1009        // (媒体) 下一曲
#define UDP_SEND_MEDIA_PREVIOUS  1010    // (媒体) 上一曲


// 文件服务命令
#define FS_SHARE_FILE  1101    // 发送文件
#define FS_AGREE  1102         // 同意
#define FS_NOT_AGREE  1103     // 不同意
#define FS_ADD_DEVICE  1104     // 添加设备
#define FS_MESSAGE  1105     // 消息
#define FS_GET_NO_SYNC_MEDIA  1106     //

#define FS_DATA  1           // 数据
#define FS_END  2            // 接收结束
#define FS_CLOSE  3          // 取消
#define FS_DATA_RECEIVED  4  // 数据接收完毕
#define FS_NEXT  5           // 下一步
#define FS_BREAK  6          // 跳出


#define FILE_IMAGE 3001       // 图片
#define FILE_VIEDO 3002       // 视频
#define FILE_FILE 3003        // 文件
#define FILE_FOLDER 3004      // 文件夹

// Service 连接命令
#define SERVICE_IF_RECIVE_FILES  1201    // 是否接收文件
#define SERVICE_SHOW_PROGRESS  1202      // 显示文件进度框
#define SERVICE_PROGRESS  1203           // 文件进度
#define SERVICE_CLOSE_PROGRESS  1204     // 关闭文件进度框
#define SERVICE_UPDATE_DEVICES  1205     // 更新设备列表

#define LOCAL_PARAMS  2000               // 本地参数


// 默认端口
#define DEFAULT_UDPPORT 4573
#define DEFAULT_TCPPORT 5856

#if defined(PLATFORM_WINDOWS)
#define DEFAULT_FILE_PATH  "C:/LANShare/"
#else
#define DEFAULT_FILE_PATH  QDir::homePath()  + "/LANShare/"
#endif

#define DEFAULT_USER_NAME QDir::home().dirName()
#define DEFAULT_MESSAGE_KEY "e4be1373272c69e0932651d97187b746c6725b17bbe84ad0b0fe2d4e81fc1d6c0c633d8ebd7f0fea65a57a9d5529d214"
#define KEY "6c9b%8ErII@Rc&f"

#define APP_NAME "LANShare"

#define FILE_PATH "filePath"
// 用户名
#define USER_NAME "userName"
#define UNIQUE_CODE "uniqueUUid"
#define CONTEXT_MENU "contextMenu"
// 使用内置工具打开媒体
#define POEN_MEDIA_PLAYER "open_media_on_internal_player"
// 接收文件无需确认
#define NOT_RECV_DIALOG "not_recv_dialog"
// 选择媒体模式
#define MEDIA_SELECT_MODEL "media_select_model"
// 保存消息
#define SAVE_MESSAGE "save_message"
// 隐私政策同意状态
#define PRIVACY_AGREE "privacy_agree"
// 隐私政策版本
#define PRIVACY_VERSION "privacy_version"
// 保存媒体到相册
#define SAVE_TO_GALLERY "save_to_gallery"
// 是否显示隐藏文件夹
#define SHOW_HIDDEN_FILES "show_hidden_files"
// tcpPort
#define TCP_PORT "tcpPort"
// udpPort
#define UDP_PORT "udpPort"
// 显示系统软件
#define DISPLAY_SYSTEM_APP "display_system_app"
// 开机自启
#define AUTO_START "auto_start"
// 广播收到的消息
#define BROADCAST_MESSASGE "broadcast_message"
// 接收系统广播消息
#define RECEIVE_BROADCAST_MESSAGES "receive_broadcast_messages"

#define SEND_MUTE "send_mute"
#define MESSAGE_KEY "messageKey"

#define RECEIVE_MUTE "receive_mute"
#define ENC_DATA "enc_data"
#define WEB_SERVICE "web_service"
#define OPEN_WEB_SERVICE "open_web_service"
#define ACCEPT_RECV_FILES "accept_recv_files"
#define ALLOW_BACKGROUND_RUNNING "allow_background_running"
#define FORCE_VERSION "force_version"
#define THEME "theme"

#define MAGIC_NUM 0x66677371
#define DATA_VERSION_1 1
#define DATA_VERSION_2 2
#define DATA_VERSION_3 3
#define LANSHARE_VERSION 2500117
#define LANSHARE_VERSION_NAME "1.1"
#define DATA_VERSION DATA_VERSION_3
#define LANSHARE_SERVER "http://lanshares.com"
//#define LANSHARE_SERVER "http://127.0.0.1:8881"
#define CACHE_DIR "/.cache"

#if defined(PLATFORM_WINDOWS)
#if defined(RELEASE)
#define APP_TYPE "release_windows"
#else
#define APP_TYPE "debug_windows"
#endif
#elif defined(PLATFORM_LINUX)
#if defined(RELEASE)
#define APP_TYPE "release_linux"
#else
#define APP_TYPE "debug_linux"
#endif
#elif defined(PLATFORM_MACOS)
#if defined(RELEASE)
#define APP_TYPE "release_macos"
#else
#define APP_TYPE "debug_macos"
#endif
#endif


#include "mUtils.h"
#include "Utils.h"
#include <string>
#include <QSettings>
#include <QDir>
#include <QtGlobal>
#include <QDebug>
#include <QCoreApplication>
#include <QStandardPaths>
#include <memory>
#include <QApplication>

class Config {
public:
    // 日志等级
    QString logInfo = "DEBUG";
    // 保存文件路径
    QString saveFilePath;
    // 客户端名称
    QString clientName;
    // 设备唯一码
    QString uniqueUUid;
    // 消息加密密钥
    QString messageKey = DEFAULT_MESSAGE_KEY;
    QString applicationDirPath = "";
    QString lanshareWorkDirPath = "";

    int tcpPort = DEFAULT_TCPPORT;
    int udpPort = DEFAULT_UDPPORT;
    // 接收静音广播
    bool receivceMute = false;
    bool encData = false;
    bool contextMenu = true;
    bool webService = true;
    bool openWebService = false;
    bool acceptRecvFiles = true;
    bool allowBackgroundRunning = false;

    std::unique_ptr<QSettings> settings;
    QString styleSheet;
    QString themeName;
    QBrush chatBubblColorLeft = Qt::white;
    QBrush chatBubblColorRight = Qt::white;

    ~Config() = default;

    Config() = default;

    void init() {
        applicationDirPath = QCoreApplication::applicationDirPath();
        qDebug() << "Config";
        // 获取当前用户目录路径
        QString userDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        lanshareWorkDirPath = userDir + "/.LANShare";
        // 创建test目录（如果不存在）
        QDir testDir(lanshareWorkDirPath);
        if (!testDir.exists()) {
            testDir.mkpath(".");
        }
        QString configFilePath = lanshareWorkDirPath + "/config.ini";
        // 创建一个QSettings对象，指定配置文件路径
        settings = std::make_unique<QSettings>(configFilePath, QSettings::IniFormat);
        saveFilePath = settings->value(FILE_PATH, DEFAULT_FILE_PATH).toString();
        clientName = settings->value(USER_NAME, DEFAULT_USER_NAME).toString();
        uniqueUUid = settings->value(UNIQUE_CODE, "").toString();
        if (uniqueUUid.isEmpty()) {
            uniqueUUid = Utils::getUUID();
            settings->setValue(UNIQUE_CODE, uniqueUUid);
        }
        tcpPort = settings->value(TCP_PORT, DEFAULT_TCPPORT).toInt();
        udpPort = settings->value(UDP_PORT, DEFAULT_UDPPORT).toInt();
        receivceMute = settings->value(RECEIVE_MUTE, false).toBool();
        encData = settings->value(ENC_DATA, false).toBool();
        contextMenu = settings->value(CONTEXT_MENU, true).toBool();
        webService = settings->value(WEB_SERVICE, true).toBool();
        openWebService = settings->value(OPEN_WEB_SERVICE, false).toBool();
        acceptRecvFiles = settings->value(ACCEPT_RECV_FILES, true).toBool();
        allowBackgroundRunning = settings->value(ALLOW_BACKGROUND_RUNNING, false).toBool();
        messageKey = settings->value(MESSAGE_KEY, DEFAULT_MESSAGE_KEY).toString();
        themeName = settings->value(THEME, "").toString();
        QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::ECB, QAESEncryption::PKCS7);
        messageKey = QString(
                QAESEncryption::RemovePadding(encryption.decode(QByteArray::fromHex(messageKey.toUtf8()), KEY),
                                              QAESEncryption::PKCS7));
        mUtils::createMultipleFolders(saveFilePath);
        mUtils::createMultipleFolders(saveFilePath + CACHE_DIR);
        setTheme(themeName);
    }

    QSettings *getSettings() const {
        return settings.get();
    }

    void setTheme(const QString &name) {
        QString newName = name;
        if (name.isEmpty()) {
            newName = mUtils::isDarkMode() ? "dark" : "light";
        }
        // 加载 QSS 文件
        QFile file(":/style/style/style_" + newName + ".qss");
        if (file.open(QFile::ReadOnly)) {
            styleSheet = QTextStream(&file).readAll();
            chatBubblColorLeft = mUtils::parseColorFromStyleSheet(
                    styleSheet,
                    "ChatBubbleLeft",
                    " background-color");
            chatBubblColorRight = mUtils::parseColorFromStyleSheet(
                    styleSheet,
                    "ChatBubbleRight",
                    " background-color");

            qApp->setStyleSheet(styleSheet);
            file.close();
        } else {
            qDebug("Could not open qss file");
        }

    }

};


extern Config config;

#endif //LANSHARE_WIN_CONFIG_H
