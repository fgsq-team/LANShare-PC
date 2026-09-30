//
// Created by fgsq on 2023/12/17.
//

#include "Request.h"
#include "IOUtils.h"

Request::Request(TCPClient *socket)
        : tcpClient(socket),
          contentLength(0),
          headerReady(false) {

    clientIP = socket->getRemoteIP();
}

Request::~Request() {
    tcpClient->close();
}

QString Request::getClientIP() {
    return clientIP;
}

void Request::setClientIP(const QString &clientIP) {
    this->clientIP = clientIP;
}


QMap<QString, QString> Request::getPathParams() {
    return pathParams;
}

QString Request::getPathParam(const QString &key) {
    return pathParams.value(key);
}


qint64 transfer(IOInter *input, QFile *output) {
    char buffer[2048];
    qint64 total = 0;
    int bytesRead = 0;
    while ((bytesRead = input->read(buffer, sizeof(buffer))) > 0) {
        qint64 bytesWritten = output->write(buffer, bytesRead);
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

UploadResult Request::readUploadBody2Stream(QString path) {
    UploadInputStream upload(tcpClient, contentLength);
    QFile file(path + upload.getFileName());
    file.open(QFile::WriteOnly);
    qint64 i = transfer(&upload, &file);
    if (i < 0) {
        throw std::runtime_error("File upload failed");
    }
    UploadResult uploadResult;
    uploadResult.setFileSize(upload.getFileSize());
    uploadResult.setFileName(upload.getFileName());
    uploadResult.setFilePath(path + upload.getFileName());
    return uploadResult;
}

UploadInputStream Request::getSingleUploadInputStream() {
    return {tcpClient, contentLength};
}


QString Request::getRequestBody() {
    if (!headerReady) {
        return {}; // Return an empty QString if headers are not ready
    }
    if (requestMethod.toUpper() == "GET") {
        return requestURLParams;
    } else {
        if (contentLength == 0) {
            return {}; // Return an empty QString if content length is 0
        }
        QByteArray buffer;
        buffer.resize(1024);
        QString requestBody;
        int size = static_cast<int>(contentLength);
        while (size > 0) {
            int read = tcpClient->recv(buffer.data(), qMin(size, buffer.size()));
            if (read == -1) {
                return {}; // Return an empty QString if an error occurs
            }
            requestBody += QString::fromUtf8(buffer.constData(), read);
            size -= read;
        }
        return requestBody;
    }
}

QVector<qint64> Request::getRangeLength() {
    QString rangeHeader = headers.value("Range");
    if (!rangeHeader.isEmpty()) {
        QStringList rangeValues = rangeHeader.mid(QString("bytes=").length()).split('-');
        qlonglong start = rangeValues[0].toLongLong();
        qlonglong end = -1;
        if (rangeValues.length() > 1 && !rangeValues[1].isEmpty()) {
            end = rangeValues[1].toLongLong();
        }
        return QVector<qlonglong>{start, end};
    }
    return QVector<qlonglong>();
}

void Request::addHeader(const QString &key, const QString &value) {
    headers.insert(key, value);
    if (key.compare("Content-Length", Qt::CaseInsensitive) == 0) {
        contentLength = value.toLongLong();
    }
}

bool Request::isHeaderReady() {
    return headerReady;
}

void Request::setHeaderReady(bool headerReady) {
    this->headerReady = headerReady;
}

QString Request::getHeaderValue(const QString &key) {
    return headers.value(key);
}

void Request::setRequestMethod(const QString &requestMethod) {
    this->requestMethod = requestMethod.toUpper();
}

const QString &Request::getRequestMethod() const {
    return requestMethod;
}

QString Request::getRequestURL() {
    return requestURL;
}

void Request::setRequestURL(const QString &requestURL) {
    this->requestURL = requestURL;
}

QString Request::getRequestURLParams() {
    return requestURLParams;
}

void Request::setRequestURLParams(const QString &requestURLParams) {
    this->requestURLParams = requestURLParams;
}

void Request::addPathParams(const QString &key, const QString &value) {
    pathParams.insert(key, value);
}

TCPClient *Request::getTcpClient() const {
    return tcpClient;
}
