//
// Created by fgsq on 2026/10/12.
//

#include "ResumeStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>

#include "Config.hpp"
#include "mUtils.h"

// 续传根目录名
static const QString RESUME_DIR = QString::fromLatin1(".lanshare_resume");
// 分段文件后缀
static const QString PART_SUFFIX = QString::fromLatin1(".part");
// 文件夹最终目录标记文件名
static const QString TARGET_MARKER = QString::fromLatin1(".target");
// 分段文件过期时间（7 天，单位毫秒）
static const qint64 PART_EXPIRE_MS = 7LL * 24 * 60 * 60 * 1000;

/**
 * 内部工具：复制源文件到目标后删除源文件。
 */
static void copyAndDelete(const QString &src, const QString &dst) {
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) {
        return;
    }
    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly)) {
        in.close();
        return;
    }
    char buffer[1024 * 1024];
    qint64 len;
    while ((len = in.read(buffer, sizeof(buffer))) > 0) {
        out.write(buffer, static_cast<int>(len));
    }
    out.flush();
    out.close();
    in.close();
    QFile::remove(src);
}

QString ResumeStore::stableId(const QString &seed) {
    QByteArray source = QByteArray("LRESUME:") + seed.toUtf8();
    return QString::fromLatin1(QCryptographicHash::hash(source, QCryptographicHash::Md5).toHex());
}

QString ResumeStore::partRoot() {
    QString root = Config::instance().saveFilePath + SEPARATORS + RESUME_DIR;
    mUtils::createMultipleFolders(root);
    return root;
}

QString ResumeStore::filePart(const QString &fileId) {
    return partRoot() + SEPARATORS + fileId + PART_SUFFIX;
}

QString ResumeStore::folderStageDir(const QString &folderFileId) {
    QString dir = partRoot() + SEPARATORS + folderFileId;
    mUtils::createMultipleFolders(dir);
    return dir;
}

QString ResumeStore::childPart(const QString &folderFileId, const QString &childFileId) {
    return partRoot() + SEPARATORS + folderFileId + SEPARATORS + childFileId + PART_SUFFIX;
}

mlong ResumeStore::readOffset(const QString &part, mlong total) {
    QFileInfo fi(part);
    if (!fi.exists()) {
        return 0;
    }
    mlong len = static_cast<mlong>(fi.size());
    if (len > total || total <= 0) {
        deleteQuietly(part);
        return 0;
    }
    return len;
}

void ResumeStore::writeTarget(const QString &stageDir, const QString &finalDirPath) {
    QString markerPath = stageDir + SEPARATORS + TARGET_MARKER;
    mUtils::createMultipleFolders(stageDir);
    QFile marker(markerPath);
    if (marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        marker.write(finalDirPath.toUtf8());
        marker.close();
    } else {
        qDebug("ResumeStore::writeTarget open error: %s", markerPath.toUtf8().constData());
    }
}

QString ResumeStore::readTarget(const QString &stageDir) {
    QString markerPath = stageDir + SEPARATORS + TARGET_MARKER;
    QFile marker(markerPath);
    if (!marker.exists() || !marker.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QByteArray data = marker.readAll();
    marker.close();
    if (data.isEmpty()) {
        return QString();
    }
    return QString::fromUtf8(data);
}

QString ResumeStore::finalizePart(const QString &part, const QString &targetFile) {
    QFileInfo targetInfo(targetFile);
    mUtils::createMultipleFolders(targetInfo.absolutePath());
    QString finalFile = mUtils::avoidDuplication(targetInfo);
    if (QFile::exists(part)) {
        if (!QFile::rename(part, finalFile)) {
            copyAndDelete(part, finalFile);
        }
    }
    return finalFile;
}

void ResumeStore::movePartToFinal(const QString &part, const QString &finalDir, const QString &relativeName) {
    if (part.isEmpty() || !QFile::exists(part)) {
        return;
    }
    QString target = finalDir + SEPARATORS + relativeName;
    QFileInfo targetInfo(target);
    mUtils::createMultipleFolders(targetInfo.absolutePath());
    if (targetInfo.exists() && targetInfo.size() == QFileInfo(part).size()) {
        // 已存在且大小一致，直接丢弃分段
        deleteQuietly(part);
        return;
    }
    if (!QFile::rename(part, target)) {
        copyAndDelete(part, target);
    }
}

bool ResumeStore::deleteQuietly(const QString &path) {
    if (path.isEmpty()) {
        return true;
    }
    QFileInfo fi(path);
    if (!fi.exists()) {
        return true;
    }
    return QFile::remove(path);
}

void ResumeStore::deleteDir(const QString &path) {
    if (path.isEmpty()) {
        return;
    }
    QDir dir(path);
    if (!dir.exists()) {
        return;
    }
    dir.removeRecursively();
}

void ResumeStore::cleanupExpired() {
    QString root = Config::instance().saveFilePath + SEPARATORS + RESUME_DIR;
    QDir dir(root);
    if (!dir.exists()) {
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::NoSort);
    for (const QFileInfo &fi: entries) {
        if (fi.lastModified().msecsTo(now) > PART_EXPIRE_MS) {
            if (fi.isDir()) {
                deleteDir(fi.absoluteFilePath());
            } else {
                QFile::remove(fi.absoluteFilePath());
            }
        }
    }
}
