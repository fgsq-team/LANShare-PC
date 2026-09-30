//
// Created by fgsqme on 2023/2/2.
//

#ifndef LANSHARE_WIN_LFILE_H
#define LANSHARE_WIN_LFILE_H

#include <string>
#include <QString>
#include "Type.h"
#include "MTCPClient.h"
#include "Config.h"


class LFile {
public:
    enum TYPE {
        FILE,
        BYTEARRAY,
        STREAM
    };
public:
    QByteArray byteArray;
    IOInter *ioInter;
    QString path;
    QString fileName;
    mlong fileSize;
    int subFileCount;
    bool directory;
    int index;
    QString uuid;
    MTCPClient *mioUtil = nullptr;
    bool nextStep = true;
    std::list<LFile> fileList;
    mlong mediaId = -1L;
    TYPE type = FILE;
    int fileType = FILE_FILE;

    LFile();

    int getFileType() const;

    void setFileType(int fileType);

    const QByteArray &getByteArray() const;

    void setByteArray(const QByteArray &byteArray);

    TYPE getType() const;

    void setType(TYPE type);

    const QString &getPath() const;

    void setPath(const QString &path);

    const QString &getFileName() const;

    void setFileName(const QString &fileName);

    mlong getFileSize() const;

    void setFileSize(mlong fileSize);

    bool isDirectory() const;

    void setIsDirectory(bool isDirectory);

    int getIndex() const;

    void setIndex(int index);

    const QString &getUuid() const;

    void setUuid(const QString &uuid);

    MTCPClient *getMioUtil() const;

    void setMioUtil(MTCPClient *mioUtil);

    IOInter *getIoInter() const;

    void setIoInter(IOInter *ioInter);

    bool isNextStep() const;

    void setNextStep(bool nextStep);

    int getSubFileCount() const;

    void setSubFileCount(int subFileCount);

    const std::list<LFile> &getFileList() const;

    void setFileList(const std::list<LFile> &fileList);

    mlong getMediaId() const;


    void setMediaId(mlong mediaId);
};


#endif //LANSHARE_WIN_LFILE_H
