
#include "Response.h"
#include "ContentTypes.h"
#include "TimeTools.h"

QString Response::HTTP_VERSION = "HTTP/1.1";
QString Response::HTML_CONTEXT_TYPE = "text/html; charset-utf-8";
QString Response::TEXT_CONTEXT_TYPE = "text/plain; charset=UTF-8";
QString Response::STREAM_CONTEXT_TYPE = "application/octet-stream";
QString Response::STREAM_CONTEXT_IMAGE = "image/png";
QString Response::STREAM_CONTEXT_JSON = "application/json";
QString Response::WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

Response::Response(TCPClient *socket) : socket(socket) {
    setStatus(200, "OK");
    addHeader("Content-Type", HTML_CONTEXT_TYPE);
    addHeader("Connection", "close");
}

void Response::setStatus(int status, const QString &statusMessage) {
    this->status = status;
    addHeader(HTTP_VERSION, QString::number(status) + " " + statusMessage);
}

void Response::setRangeLength(QVector<qint64> rangeLength) {
    this->rangeLength = rangeLength;
}

void Response::setContentRange(qint64 start, qint64 end, qint64 length) {
    setStatus(206, "Partial Content");
    addHeader("Accept-Ranges", "bytes");
    addHeader("Content-Range", "bytes " + QString::number(start) + "-" + QString::number(end - 1) + "/" +
                               QString::number(length + start));
    setContentLength(length);
}

void Response::setContentType(const QString &contentType) {
    addHeader("Content-Type", contentType);
}

void Response::setContentLength(qint64 contentLength) {
    this->contentLength = contentLength;
    addHeader("Content-Length", QString::number(contentLength));
}

void Response::addHeader(const QString &name, const QString &value) {
    headers.insert(name, value);
}

void Response::writeString(const QString &responseBody, const QString &contentType) {
    writeBytes(responseBody.toUtf8(), contentType);
}

void Response::writeString(const QString &responseBody) {
    writeString(responseBody, STREAM_CONTEXT_JSON);
}

void Response::writeBytes(const QByteArray &bytes, const QString &contentType) {
    setContentLength(bytes.size());
    setContentType(contentType);
    QByteArray data = createHeader().toUtf8() + bytes;
//    qDebug() << "data:" << data.data();
    socket->send(data.data(), data.size());
}

qint64 transferData(QFile *input, TCPClient *output) {
    char buffer[2048];
    qint64 total = 0;
    int bytesRead = 0;
    while ((bytesRead = input->read(buffer, sizeof(buffer))) > 0) {
        qint64 bytesWritten = output->send(buffer, bytesRead);
        if (bytesWritten < 0) {
            // 处理写入错误
            return -1;
        }
        total += bytesWritten;
        // 如果写入的字节数不等于读取的字节数，则表示写入错误
        if (bytesWritten != bytesRead) {
            // 处理写入错误
            return -1;
        }
    }
    return total;
}

qint64 transferData(QFile *input, TCPClient *output, qint64 size) {
    char buffer[2048];
    qint64 total = 0;
    int bytesRead = 0;
    while ((bytesRead = input->read(buffer, qMin<qint64>(2048, size - total))) > 0) {
        qint64 bytesWritten = output->send(buffer, bytesRead);
        if (bytesWritten < 0) {
            // 处理写入错误
            return -1;
        }
        total += bytesWritten;
        // 如果写入的字节数不等于读取的字节数，则表示写入错误
        if (bytesWritten != bytesRead) {
            // 处理写入错误
            return -1;
        }
    }
    return total;
}

void Response::writeFile(QFile &file) {
    if (file.open(QIODevice::ReadOnly)) {
        setContentLength(file.size());
        QString name = file.fileName();
        QString contentTypeByName = getContentTypeByName(name);
        if (!contentTypeByName.isEmpty()) {
            setContentType(contentTypeByName);
        } else {
            setContentType(STREAM_CONTEXT_TYPE);
        }
        mlong rangeContentLength = 0;
        if (rangeLength.isEmpty()) {
            setStatus(200, "OK");
            QByteArray data = createHeader().toUtf8();
            socket->send(data.data(), data.size());
            transferData(&file, socket);
        } else {
            qint64 start = rangeLength[0];
            qint64 end = rangeLength[1];
            if (end == -1) {
                end = file.size();
            }
            rangeContentLength = end - start;
            setContentRange(start, end, rangeContentLength);
            QByteArray data = createHeader().toUtf8();
            socket->send(data.data(), data.size());
            file.seek(start);
            transferData(&file, socket, end - start);
        }
    } else {
        write404();
    }
}


void Response::writeFile(const QString &filePath) {
    QFile file(filePath);
    writeFile(file);
}

void Response::writeWebSocket(const QString &webSocketKey) {
    if (!webSocketKey.isEmpty()) {
        QByteArray concatenated = (webSocketKey + WS_MAGIC).toUtf8();
        QByteArray digest = QCryptographicHash::hash(concatenated, QCryptographicHash::Sha1);
        QString handshakeAccept = QString::fromUtf8(digest.toBase64());
        qDebug()<<"concatenated:"<<concatenated;
        qDebug()<<"digest:"<<digest;
        qDebug()<<"handshakeAccept:"<<handshakeAccept;
        QString response = "HTTP/1.1 101 Switching Protocols\r\n"
                           "Upgrade: websocket\r\n"
                           "Connection: Upgrade\r\n"
                           "Sec-WebSocket-Accept: " + handshakeAccept + "\r\n\r\n";
        auto data = response.toUtf8();
        socket->send(data.data(), data.size());
    }
}

QString Response::createHeader() {
    QString code = headers[HTTP_VERSION];
    QString header;
    QTextStream stream(&header);
    stream << HTTP_VERSION << " " << code << "\r\n";
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        if (it.key() != HTTP_VERSION) {
            stream << it.key() << ": " << it.value() << "\r\n";
        }
    }
    stream << "\r\n";
    return header;
}


QString Response::getContentTypeByName(const QString &name) {
    int i = name.lastIndexOf(".");
    if (i > 0) {
        QString suffix = name.mid(i + 1);
        QString contentType = ContentTypes::contentTypeMap.value(suffix);
        if (!contentType.isEmpty()) {
            return contentType;
        }
    }
    return {};
}

void Response::write404() {
    QString responseBody = "404 Not Found";
    setStatus(404, "Not Found");
    writeBytes(responseBody.toUtf8(), TEXT_CONTEXT_TYPE);
}

void Response::write500(const QString &message) {
    setStatus(500, "error");
    writeBytes(message.toUtf8(), TEXT_CONTEXT_TYPE);
}

void Response::write500() {
    write500("500 error");
}

void Response::write302(const QString &message, const QString &url) {
    setStatus(302, "Found");
    addHeader("Location", url);
    writeBytes(message.toUtf8(), TEXT_CONTEXT_TYPE);
}

void Response::write405() {
    QString responseBody = "405 Method Not Allowed";
    setStatus(404, "Method Not Allowed");
    writeBytes(responseBody.toUtf8(), TEXT_CONTEXT_TYPE);
}

