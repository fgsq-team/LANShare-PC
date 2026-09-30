//
// Created by fgsqme on 2023/2/2.
//

#include "LFile.h"


LFile::LFile() {
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

const std::list<LFile> &LFile::getFileList() const {
    return fileList;
}

void LFile::setFileList(const std::list<LFile> &fileList) {
    LFile::fileList = fileList;
}

mlong LFile::getMediaId() const {
    return mediaId;
}

void LFile::setMediaId(mlong mediaId) {
    LFile::mediaId = mediaId;
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



