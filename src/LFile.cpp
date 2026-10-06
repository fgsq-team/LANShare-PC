//
// Created by fgsqme on 2023/2/2.
//

#include "LFile.h"

#include "CustomDataStream.h"


LFile::LFile() {
}

// 发送某个文件取消传输指令
void LFile::cancelFileTransfer() {
    nextStep = false;
    if (customDataStream == nullptr) {
        return;
    }
    customDataStream->writeString(fileId.toStdString());
}

const QString &LFile::getPath() const {
    return path;
}

void LFile::setPath(const QString &path) {
    LFile::path = path;
}

const QString &LFile::getFileName() const {
    return fileName;
}

void LFile::setFileName(const QString &fileName) {
    LFile::fileName = fileName;
}

mlong LFile::getFileSize() const {
    return fileSize;
}

void LFile::setFileSize(mlong fileSize) {
    LFile::fileSize = fileSize;
}

bool LFile::isDirectory() const {
    return directory;
}

void LFile::setIsDirectory(bool directory) {
    LFile::directory = directory;
    if (directory) {
        fileType = FILE_FOLDER;
    }
}

int LFile::getIndex() const {
    return index;
}

void LFile::setIndex(int index) {
    LFile::index = index;
}

const QString &LFile::getUuid() const {
    return uuid;
}

void LFile::setUuid(const QString &uuid) {
    LFile::uuid = uuid;
}

MTCPClient *LFile::getMioUtil() const {
    return mioUtil;
}

void LFile::setMioUtil(MTCPClient *mioUtil) {
    LFile::mioUtil = mioUtil;
}


bool LFile::isNextStep() const {
    return nextStep;
}

void LFile::setNextStep(bool nextStep) {
    this->nextStep = nextStep;
}

int LFile::getSubFileCount() const {
    return subFileCount;
}

void LFile::setSubFileCount(int subFileCount) {
    LFile::subFileCount = subFileCount;
}

const std::vector<LFile> &LFile::getFileList() const {
    return fileList;
}

void LFile::setFileList(const std::vector<LFile> &fileList) {
    LFile::fileList = fileList;
}

mlong LFile::getMediaId() const {
    return mediaId;
}

void LFile::setMediaId(mlong mediaId) {
    LFile::mediaId = mediaId;
}

void LFile::setProgress(int progress) {
    LFile::progress = progress;
}

int LFile::getProgress() const {
    return progress;
}

void LFile::setCustomDataStream(CustomDataStream *customDataStream) {
    LFile::customDataStream = customDataStream;
}

CustomDataStream *LFile::getCustomDataStream() const {
    return customDataStream;
}

QString LFile::getFileId() const {
    return fileId;
}

void LFile::setFileId(const QString &fileId) {
    LFile::fileId = fileId;
}

LFile::TYPE LFile::getType() const {
    return type;
}

void LFile::setType(LFile::TYPE type) {
    LFile::type = type;
}

const QByteArray &LFile::getByteArray() const {
    return byteArray;
}

void LFile::setByteArray(const QByteArray &byteArray) {
    LFile::byteArray = byteArray;
    type = BYTEARRAY;
}

int LFile::getFileType() const {
    return fileType;
}

void LFile::setFileType(int fileType) {
    LFile::fileType = fileType;
}

IOInter *LFile::getIoInter() const {
    return ioInter;
}

void LFile::setIoInter(IOInter *ioInter) {
    LFile::ioInter = ioInter;
}

mlong LFile::findFile(std::vector<LFile> &listFile, mlong size, const QString &path) {
    QDir dir(path);
    qDebug() << "扫描路径:" << path;
    if (!dir.exists())
        return false;
    dir.setFilter(QDir::Dirs | QDir::Files);
    //    dir.setSorting(QDir::DirsFirst);
    QFileInfoList list = dir.entryInfoList();
    int i = 0;
    mlong fileSize = size;
    do {
        const QFileInfo &fileInfo = list.at(i);
        if (fileInfo.fileName() == "." | fileInfo.fileName() == "..") {
            i++;
            continue;
        }
        if (fileInfo.isDir()) {
            fileSize += findFile(listFile, 0, fileInfo.filePath());
        } else {
            LFile file;
            file.setFileName(fileInfo.fileName());
            file.setIsDirectory(false);
            file.setFileSize(fileInfo.size());
            file.setPath(fileInfo.filePath());
            listFile.push_back(file);
            fileSize += fileInfo.size();
            qDebug() << "扫描到文件: " + fileInfo.filePath() << "大小:" << fileInfo.size();
        }
        i++;
    } while (i < list.size());
    return fileSize;
}

mlong LFile::findFileNew(std::vector<LFile> &listFile, mlong size, const QString &path, const QString &basePath) {
    QDir dir(path);
    qDebug() << "扫描路径:" << path;
    if (!dir.exists())
        return false;
    dir.setFilter(QDir::Dirs | QDir::Files);
    //    dir.setSorting(QDir::DirsFirst);
    QFileInfoList list = dir.entryInfoList();
    int i = 0;
    mlong fileSize = size;
    do {
        const QFileInfo &fileInfo = list.at(i);
        if (fileInfo.fileName() == "." | fileInfo.fileName() == "..") {
            i++;
            continue;
        }
        if (fileInfo.isDir()) {
            fileSize += findFileNew(listFile, 0, fileInfo.filePath(), basePath);
        } else {
            QString relativePath = fileInfo.absoluteFilePath().remove(0, basePath.length() + 1);
            LFile file;
            file.setFileName(relativePath);
            file.setIsDirectory(false);
            file.setFileSize(fileInfo.size());
            file.setPath(fileInfo.filePath());
            listFile.push_back(file);
            fileSize += fileInfo.size();
            qDebug() << "扫描到文件: " + fileInfo.filePath() << "大小:" << fileInfo.size();
        }
        i++;
    } while (i < list.size());
    return fileSize;
}
