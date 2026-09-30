#include "UploadResult.h"

UploadResult::UploadResult() : fileSize(0) {}

QString UploadResult::getFilePath() const {
    return filePath;
}

void UploadResult::setFilePath(const QString &filePath) {
    this->filePath = filePath;
}

QString UploadResult::getFileName() const {
    return fileName;
}

void UploadResult::setFileName(const QString &fileName) {
    this->fileName = fileName;
}

qint64 UploadResult::getFileSize() const {
    return fileSize;
}

void UploadResult::setFileSize(qint64 fileSize) {
    this->fileSize = fileSize;
}
