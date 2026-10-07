#include "UploadInputStream.h"
#include "TCPClient.h"
#include <QFile>
#include <QUuid>
#include <QDebug>

static const int MAX_HTTP_LINE_LENGTH = 8192;

UploadInputStream::~UploadInputStream() = default;

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
            int filenameIdx = lineUpperCase.indexOf("FILENAME=\"");
            if (filenameIdx != -1) {
                filename = line.mid(filenameIdx + 10);
                int endQuoteIdx = filename.indexOf("\"");
                if (endQuoteIdx != -1) {
                    filename = filename.mid(0, endQuoteIdx);
                }
            }
        }
        if (lineUpperCase.startsWith("CONTENT-TYPE")) {
            if (filename.isEmpty()) {
                filename = QUuid::createUuid().toString();
            }
            this->fileName = filename;
            is->skip(2); // \r\n 空行分隔
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
        return 0;
    }
    qint64 remainingSize = fileSize - readSize;
    int bytesRead = is->recv(data, (int) qMin((qint64) maxlen, remainingSize));
    if (bytesRead > 0) {
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
    while (byteArray.length() < MAX_HTTP_LINE_LENGTH) {
        char c;
        int r = is->read();
        if (r == -1) {
            return {};
        }
        c = static_cast<char>(r);
        byteArray.append(c);
        if (c == '\r') {
            r = is->read();
            if (r == -1) {
                return {};
            }
            c = static_cast<char>(r);
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

