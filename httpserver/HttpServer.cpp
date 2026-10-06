//
// Created by fgsq on 2023/12/17.
//

#include "HttpServer.h"
#include "TCPServer.h"
#include "TCPClient.h"
#include "TimeTools.h"
#include "Request.h"
#include "Response.h"
#include "PathEntry.h"
#include "Utils.h"
#include <QDebug>
#include <QUrl>
#include <QRegularExpression>
#include <thread>

QString readHttpLine(TCPClient *is) {
    QString sb;
    while (true) {
        int read = is->read();
        if (read == -1) {
            return {};
        }
        char c = static_cast<char>(read);
        if (c == '\r') {
            c = static_cast<char>(is->read());
            if (c == '\n') {
                break;
            } else {
                sb.append('\r');
                sb.append(c);
            }
        } else {
            sb.append(c);
        }
    }
    return sb;
}

bool HttpServer::pathMatches(const QString &registeredPathPattern, const QString &requestPath) {
    // 使用通配符 "*" 进行路径匹配
    QString regexPattern = registeredPathPattern;
    regexPattern.replace("*", "[^/]*"); // 匹配除了 '/' 之外的任何字符
    if (!regexPattern.endsWith("[^/]")) {
        // 如果模式不以 "[^/]" 结尾，添加 "$" 以确保只匹配整个路径段
        regexPattern += "$";
    }
    QRegularExpression regex(regexPattern);
    QRegularExpressionMatch match = regex.match(requestPath);
    // 返回是否匹配成功
    return match.hasMatch();
}


HttpServer::HttpServer() = default;

HttpServer::HttpServer(int port) : port(port) {
}

void HttpServer::addPath(const QString &path, HttpHandler handler) {
    handlerMap.append({path, handler});
}

void HttpServer::addPath(const QString &path, const QString &method, HttpHandler handler) {
    handlerMap.append({path, method, handler});
}

void HttpServer::sortPath() {
    std::sort(handlerMap.begin(), handlerMap.end(),
              [](const PathEntry &o1, const PathEntry &o2) {
                  return o1.getPath().length() - o2.getPath().length();
              });
}

void HttpServer::startServer() {
    sortPath();
    TCPServer tcpServer(port);
    if (!tcpServer.bind()) {
        return;
    }
    std::unique_ptr<TCPClient> tcpClient = nullptr;
    auto self = shared_from_this();
    while ((tcpClient = tcpServer.accept()) != nullptr) {
        std::thread([tcpClient = std::move(tcpClient), self]() mutable {
            self->newClient(tcpClient.get(), {});
        }).detach();
    }
}

QString formatUrl(const QString &url) {
    LString decoded = Utils::urlDecode(url.toUtf8().constData());
    return QString::fromUtf8(decoded.getCString(), decoded.getLength());
}

void HttpServer::newClient(TCPClient *tcpClient, const QString &method) {
    auto request = std::make_unique<Request>(tcpClient);
    QString line;
    int lineNum = 0;
    while (!(line = readHttpLine(tcpClient)).isNull()) {
        if (line.isEmpty()) {
            break;
        }
        // 已经读取的字符需要再拼接回来
        if (lineNum == 0 && !method.isNull()) {
            line = method + line;
        }
        int index = line.indexOf(" ");
        // 请求头数据
        QString key = line.left(index);
        QString value = line.mid(index + 1);
        if (lineNum == 0) {
            QStringList s = value.split(" ");
            const QString &url = s.at(0);
            int i = url.indexOf("?");
            if (i > 0) {
                QString requestURL = url.left(i);
//                qDebug() << "url:" << formatUrl(url);
                request->setRequestURL(formatUrl(requestURL));
                request->setRequestURLParams(url.mid(i + 1));
                if (!request->getRequestURLParams().isEmpty()) {
                    QStringList split = request->getRequestURLParams().split("&");
                    for (const QString &p: split) {
                        if (p.contains("=")) {
                            QString paramsKey = p.left(p.indexOf("="));
                            QString paramsValue = p.mid(p.indexOf("=") + 1);
//                            qDebug() << "paramsKey:" << formatUrl(paramsKey);
//                            qDebug() << "paramsValue:" << formatUrl(paramsValue);
                            request->addPathParams(formatUrl(paramsKey), formatUrl(paramsValue));
                        }
                    }
                }
            } else {
                // 没有请求参数
                // 请求路径
                request->setRequestURL(formatUrl(url));
            }
            request->setRequestMethod(key);
            request->addHeader(key, value);
//            qDebug() << "requestURL: " << request->getRequestURL();
        } else {
            int i = key.indexOf(":");
            if (i > 0) {
                key = key.left(i);
            }
            request->addHeader(key, value);
        }
//        qDebug() << "key: " << key << " value: " << value;
        lineNum++;
    }
    // 请求头解析完毕
    request->setHeaderReady(true);
//    qDebug() << "end";
    auto response = std::make_unique<Response>(tcpClient);
    response->setRangeLength(request->getRangeLength());
    bool isMatch = false;
    for (const auto &handle: handlerMap) {
        QString path = handle.getPath();
        isMatch = pathMatches(path, request->getRequestURL());
        if (isMatch) {
            if (!handle.getMethod().isEmpty() && request->getRequestMethod() != handle.getMethod()) {
                response->write405();
                break;
            }
            HttpHandler handler = handle.getHttpHandler();
            try {
                if (requestFilter != nullptr) {
                    requestFilter(request.get(), response.get(), handler);
                } else {
                    handler(request.get(), response.get());
                }
            } catch (const std::exception &e) {
                response->write500();
            }
            break;
        }
    }
    if (!isMatch) {
        response->write404();
    }
    // 优雅关闭：先半关闭输出流通知客户端数据已发送完毕，
    // 等待客户端确认接收完成后再彻底关闭 Socket
    gracefulClose(tcpClient);
}

void HttpServer::setRequestFilter(RequestFilter requestFilter) {
    HttpServer::requestFilter = requestFilter;
}

/**
 * 优雅关闭 Socket 连接
 * 先半关闭输出流通知客户端数据已发送完毕，等待客户端关闭连接后再彻底关闭 Socket
 * 替代固定延时，确保客户端能完整接收响应数据
 * @param tcpClient 客户端 TCPClient 指针
 */
void HttpServer::gracefulClose(TCPClient *tcpClient) {
    if (!tcpClient) return;
    int fd = tcpClient->getFd();
    if (fd < 0) return;

    // 半关闭输出流：发送 FIN 通知客户端"数据已写完"
    // 此时输入流仍然打开，可以等待客户端的响应
#if defined(PLATFORM_WINDOWS)
    shutdown(fd, SD_SEND);
#else
    shutdown(fd, SHUT_WR);
#endif

    // 设置 2s 读超时兜底，防止异常客户端不关闭导致线程卡死
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));

    // 等待客户端关闭连接（recv 返回 0 表示对端已关闭）
    // 丢弃客户端可能发送的残余数据
    char buf[256];
    while (tcpClient->recv(buf, sizeof(buf)) > 0) {}

    // 彻底关闭 Socket
    tcpClient->close();
}
