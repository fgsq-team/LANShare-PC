#include "UploadInputStream.h"
#include "TCPClient.h"
#include <QFile>
#include <QUuid>
#include <QDebug>

UploadInputStream::~UploadInputStream() {
}

UploadInputStream::UploadInputStream(TCPClient *is, qint64 contentLength) :
        is(is), fileName(), fileSize(0),
        readSize(0) {
    qint64 firstLineLength = -1;
    qint64 readHeadLength = 0;
    QString filename;
    bool ready = false;
    while (true) {
        QByteArray startFlagBytes = readHttpLineByte(is);
        if (startFlagBytes.isNull()) break;
        if (firstLineLength == -1) {
            firstLineLength = startFlagBytes.length() + 4;
        }
        readHeadLength += startFlagBytes.length();
        QString line = QString::fromUtf8(startFlagBytes);
        QString lineUpperCase = line.toUpper();
        if (lineUpperCase.startsWith("CONTENT-DISPOSITION")) {
            try {
                filename = line.mid(lineUpperCase.indexOf("FILENAME=\"") + 10);
                filename = filename.mid(0, filename.indexOf("\""));
            } catch (const std::exception &e) {
                qDebug() << "Error: " << e.what();
            }
        }
        if (lineUpperCase.startsWith("CONTENT-TYPE")) {
            if (filename.isNull()) {
                filename = QUuid::createUuid().toString();
            }
            this->fileName = filename;
            is->skip(2); // \r\n
            readHeadLength += 2;
            fileSize = contentLength - (firstLineLength + readHeadLength);
            ready = true;
            break;
        }
    }
    if (!ready) {
        throw std::runtime_error("File upload failed");
    }
}

int UploadInputStream::read(void *data, int maxlen) {
    if (readSize >= fileSize) {
        return -1;
    }
    qint64 remainingSize = fileSize - readSize;
    int bytesRead = is->recv(data, (int) qMin((qint64) maxlen, remainingSize));
    if (bytesRead != -1) {
        readSize += bytesRead;
    }
    return bytesRead;
}

QString UploadInputStream::getFileName() const {
    return fileName;
}

int64_t UploadInputStream::getFileSize() {
    return fileSize;
}

qint64 UploadInputStream::getReadSize() const {
    return readSize;
}

void UploadInputStream::close() {
//    is->close();
}

mlong UploadInputStream::setSeek(mlong off) {
    return 0;
}

mlong UploadInputStream::getSeek() {
    return 0;
}

QByteArray UploadInputStream::readHttpLineByte(TCPClient *is) {
    QByteArray byteArray;
    while (true) {
        char c;
        if ((c = is->read()) == -1) {
            return {};
        }
        byteArray.append(c);
        if (c == '\r') {
            c = is->read();
            byteArray.append(c);
            if (c == '\n') {
                break;
            }
        }
    }
    return byteArray;
}

int UploadInputStream::write(const char *buff, int len) {
    return 0;
}

