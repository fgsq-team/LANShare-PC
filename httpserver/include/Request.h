//
// Created by fgsq on 2023/12/17.
//

#ifndef LANSHARE_REQUEST_H
#define LANSHARE_REQUEST_H


#include <QMap>
#include <QString>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QDebug>
#include "TCPClient.h"
#include "UploadResult.h"
#include "UploadInputStream.h"

class Request : public QObject {
Q_OBJECT

public:
    explicit Request(TCPClient *socket);

    ~Request();

    QString getClientIP();

    void setClientIP(const QString &clientIP);

    QMap<QString, QString> getPathParams();

    QString getPathParam(const QString &key);

    QString getRequestBody();

    QVector<qint64> getRangeLength();

    void addHeader(const QString &key, const QString &value);

    bool isHeaderReady();

    void setHeaderReady(bool headerReady);

    QString getHeaderValue(const QString &key);

    void setRequestMethod(const QString &requestMethod);

    QString getRequestURL();

    void setRequestURL(const QString &requestURL);

    QString getRequestURLParams();

    void setRequestURLParams(const QString &requestURLParams);

    void addPathParams(const QString &key, const QString &value);

    TCPClient *getTcpClient() const;

    UploadResult readUploadBody2Stream(QString path);

    UploadInputStream getSingleUploadInputStream();

    const QString &getRequestMethod() const;

private:
    TCPClient *tcpClient;
    QString clientIP;
    qint64 contentLength;
    QMap<QString, QString> headers;
    QString requestURL;
    QString requestURLParams;
    QMap<QString, QString> pathParams;
    QString requestMethod;
    bool headerReady;
};


#endif //LANSHARE_REQUEST_H
