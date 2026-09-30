//
// Created by fgsq on 2023/12/17.
//

#ifndef LANSHARE_RESPONSE_H
#define LANSHARE_RESPONSE_H

#include <QFile>
#include <QDataStream>
#include <QCryptographicHash>
#include <QByteArray>
#include <QDebug>
#include "TCPClient.h"

class Response : public QObject {
Q_OBJECT


public:
    Response(TCPClient *socket);

    void setStatus(int status, const QString &statusMessage);

    void setRangeLength(QVector<qint64> rangeLength);

    void setContentRange(qint64 start, qint64 end, qint64 length);

    void setContentType(const QString &contentType);

    void setContentLength(qint64 contentLength);

    void addHeader(const QString &name, const QString &value);

    void writeString(const QString &responseBody, const QString &contentType);

    void writeString(const QString &responseBody);

    void writeBytes(const QByteArray &bytes, const QString &contentType);

    void writeFile(const QString &filePath);

    void writeFile(QFile &file);

    void writeWebSocket(const QString &webSocketKey);

    void write404();

    void write405();

    void write500();

    void write500(const QString &message);

    void write302(const QString &message, const QString &url);

public:
    static QString HTTP_VERSION;
    static QString HTML_CONTEXT_TYPE;
    static QString TEXT_CONTEXT_TYPE;
    static QString STREAM_CONTEXT_TYPE;
    static QString STREAM_CONTEXT_IMAGE;
    static QString STREAM_CONTEXT_JSON;
    static QString WS_MAGIC;

private:
    QString createHeader();

    static QString getContentTypeByName(const QString &name);

    TCPClient *socket;
    QMap<QString, QString> headers;
    qint64 contentLength = 0;
    QVector<qint64> rangeLength;
    int status = 200;

};


#endif //LANSHARE_RESPONSE_H
