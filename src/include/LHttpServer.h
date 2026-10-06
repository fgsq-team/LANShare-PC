//
// Created by fgsqme on 2024/5/7.
//

#ifndef LANSHARE_LHTTPSERVER_H
#define LANSHARE_LHTTPSERVER_H

#include <memory>
#include <QJsonArray>
#include "HttpServer.h"

class LANShare;

/**
 * HTTP/WebSocket 服务类
 * 负责网页端的文件浏览、消息收发、设备列表同步、主题推送等功能
 * @author fgsq
 * @version 1.0
 */
class LHttpServer {
private:
    LANShare *lanShare;
    static QJsonArray webMenus;
public:
    std::shared_ptr<HttpServer> httpServer;
public:
    /**
     * 构造函数
     * @param lanShare LANShare 实例指针
     */
    LHttpServer(LANShare *lanShare);

    /** 向所有 WebSocket 客户端推送设备列表 */
    static void sendDeviceList();

    /**
     * 向所有 WebSocket 客户端推送聊天消息
     * @param message 消息内容
     * @param toDevName 目标设备名
     * @param filePath 文件路径
     * @param messageType 消息类型
     * @param devType 设备类型
     * @param fileSize 文件大小
     * @param isLeft 是否左侧气泡
     * @param isClip 是否剪切板消息
     * @param isFile 是否文件消息
     */
    static void sendWebSocketMessage(
            const QString &message, const QString &toDevName, const QString &filePath,
            int messageType, int devType, const QString &fileSize,
            bool isLeft, bool isClip, bool isFile = true
    );

    /** 向所有 WebSocket 客户端推送主题变更通知 */
    static void sendTheme();

    /** 析构函数 */
    ~LHttpServer();
};


#endif //LANSHARE_LHTTPSERVER_H
