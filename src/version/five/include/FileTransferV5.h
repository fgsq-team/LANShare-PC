//
// Created by fgsq on 2026/10/12.
//

#ifndef LANSHARE_FILETRANSFERV5_H
#define LANSHARE_FILETRANSFERV5_H

#include <map>

#include "FileTransfer.h"
#include "Type.h"

/**
 * V5 文件传输对象
 * <p>在 V4 {@link FileTransfer} 基础上增量携带断点续传协商信息：</p>
 * <ul>
 *     <li>{@code resumeEnabled}：本次传输是否启用续传（双端均为 V5）</li>
 *     <li>{@code resumeMap}：接收端回传的每个 fileId 已完成字节偏移</li>
 * </ul>
 *
 * @author fgsq
 * @version 5.0
 */
class FileTransferV5 : public FileTransfer {
public:
    /** 是否启用断点续传 */
    bool resumeEnabled = false;

    /** fileId -> 已完成字节偏移 */
    std::map<QString, mlong> resumeMap;

    /**
     * 获取指定 fileId 的续传偏移量。
     * @param fileId 文件稳定 id
     * @return 已完成字节数，无记录返回 0
     */
    mlong getResumeOffset(const QString &fileId) const {
        if (fileId.isEmpty()) {
            return 0;
        }
        auto it = resumeMap.find(fileId);
        return it == resumeMap.end() ? 0 : it->second;
    }

    void setResumeMap(const std::map<QString, mlong> &map) {
        resumeMap = map;
    }
};

#endif //LANSHARE_FILETRANSFERV5_H
