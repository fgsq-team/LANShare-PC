//
// Created by fgsq on 2026/10/12.
//

#ifndef LANSHARE_RESUMESTORE_H
#define LANSHARE_RESUMESTORE_H

#include <QString>

#include "Type.h"

/**
 * V5 断点续传存储工具
 * <p>以 {@code .part} 分段文件的形式保存未传输完成的字节，偏移量由分段文件当前长度推导，
 * 天然跨进程/重启持久，无需数据库。</p>
 * <p>目录结构：</p>
 * <pre>
 *   {saveFilePath}/.lanshare_resume/
 *       {fileId}.part                     // 单文件分段
 *       {folderFileId}/                    // 文件夹暂存目录
 *           .target                        // 记录该文件夹的最终落地目录
 *           {childFileId}.part            // 文件夹内子文件分段
 * </pre>
 *
 * @author fgsq
 * @version 5.0
 */
class ResumeStore {
public:
    /**
     * 基于种子生成稳定 fileId（跨重启一致），用于命名分段文件。
     * @param seed 种子（发送设备 UUID + 路径 + 长度等）
     * @return 稳定的 32 位十六进制摘要
     */
    static QString stableId(const QString &seed);

    /**
     * @return 续传根目录（自动创建）
     */
    static QString partRoot();

    /**
     * 单文件分段路径。
     * @param fileId 稳定 fileId
     * @return 分段文件绝对路径
     */
    static QString filePart(const QString &fileId);

    /**
     * 文件夹暂存目录（自动创建）。
     * @param folderFileId 文件夹稳定 fileId
     * @return 暂存目录绝对路径
     */
    static QString folderStageDir(const QString &folderFileId);

    /**
     * 文件夹内子文件分段路径。
     * @param folderFileId 文件夹稳定 fileId
     * @param childFileId  子文件稳定 fileId
     * @return 子文件分段绝对路径
     */
    static QString childPart(const QString &folderFileId, const QString &childFileId);

    /**
     * 读取分段文件当前已完成字节数；若分段文件损坏（超过总长度）则删除并返回 0。
     * @param part   分段文件路径
     * @param total  目标文件总长度
     * @return 已完成字节数
     */
    static mlong readOffset(const QString &part, mlong total);

    /**
     * 记录文件夹最终落地目录，保证续传时复用同一目录，避免拆分。
     * @param stageDir     暂存目录
     * @param finalDirPath 最终目录绝对路径
     */
    static void writeTarget(const QString &stageDir, const QString &finalDirPath);

    /**
     * 读取文件夹最终落地目录。
     * @param stageDir 暂存目录
     * @return 最终目录绝对路径；不存在返回空串
     */
    static QString readTarget(const QString &stageDir);

    /**
     * 将分段文件完成化为最终文件（优先重命名，失败回退复制删除）。
     * @param part       分段文件路径
     * @param targetFile 目标文件（未去重）
     * @return 实际写入的目标文件路径
     */
    static QString finalizePart(const QString &part, const QString &targetFile);

    /**
     * 将子文件分段移动到最终目录内的相对路径处。
     * @param part         子文件分段路径
     * @param finalDir     最终目录
     * @param relativeName 子文件相对名称（可能含子目录）
     */
    static void movePartToFinal(const QString &part, const QString &finalDir, const QString &relativeName);

    /**
     * 删除文件（静默失败）。
     * @param path 目标文件路径
     * @return 是否删除成功（或本就不存在）
     */
    static bool deleteQuietly(const QString &path);

    /**
     * 递归删除目录。
     * @param path 目录路径
     */
    static void deleteDir(const QString &path);

    /**
     * 清理过期的分段文件与暂存目录。
     */
    static void cleanupExpired();

private:
    ResumeStore() = delete;
};

#endif //LANSHARE_RESUMESTORE_H
