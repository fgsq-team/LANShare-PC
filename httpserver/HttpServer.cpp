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
#include <regex>
std::vector<PathEntry> handlerMap;

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

bool HttpServer::pathMatches(const std::string &registeredPathPattern, const std::string &requestPath) {
    // 使用通配符 "*" 进行路径匹配
    std::string regexPattern = registeredPathPattern;
    std::regex wildcard("\\*");
    regexPattern = std::regex_replace(regexPattern, wildcard, "[^/]*"); // 匹配除了 '/' 之外的任何字符
    if (!regexPattern.empty() && regexPattern.back() != '/' && regexPattern.back() != '$') {
        // 如果模式不以 "[^/]" 结尾，添加 "$" 以确保只匹配整个路径段
        regexPattern += "$";
    }
    std::regex regex(regexPattern);
    std::smatch match;
    // 返回是否匹配成功
    return std::regex_search(requestPath, match, regex);
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
    while ((tcpClient = tcpServer.accept()) != nullptr) {
        std::thread([tcpClient = std::move(tcpClient), httpServer = this]() mutable {
            httpServer->newClient(tcpClient.get(), {});
            tcpClient->close();
        }).detach();
    }
}

QString formatUrl(const QString &url) {
    return Utils::urlDecode(url.toUtf8().constData()).getCString();
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
    boolean isMatch = false;
    for (const auto &handle: handlerMap) {
        QString path = handle.getPath();
        isMatch = pathMatches(path.toStdString(), request->getRequestURL().toStdString());
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
    if (request->getRequestMethod() == "POST") {
        TimeTools::sleep_s(1);
    }
    // tcpClient->close();
}

void HttpServer::setRequestFilter(RequestFilter requestFilter) {
    HttpServer::requestFilter = requestFilter;
}
