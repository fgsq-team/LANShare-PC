//
// Created by fgsqme on 2023/12/23.
//

#ifndef LANSHARE_UPLOADRESULT_H
#define LANSHARE_UPLOADRESULT_H


#include <QString>

class UploadResult {
public:
    UploadResult();

    QString getFilePath() const;

    void setFilePath(const QString &filePath);

    QString getFileName() const;

    void setFileName(const QString &fileName);

    qint64 getFileSize() const;

    void setFileSize(qint64 fileSize);

private:
    QString filePath;
    QString fileName;
    qint64 fileSize;
};

#endif //LANSHARE_UPLOADRESULT_H
