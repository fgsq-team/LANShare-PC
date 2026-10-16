//
// Created by 16508 on 2025/8/30.
//

#ifndef LANSHARE_FILETRANSFER_H
#define LANSHARE_FILETRANSFER_H
#include "Device.h"
#include <vector>

#include "LFile.h"

class FileTransfer {
public:
    Device fromDevice;
    Device toDevice;
    int type;
    QString groupId;
    std::vector<LFile*> files;

    // 允许 V5 派生对象通过基类指针安全删除
    virtual ~FileTransfer() = default;
};


#endif //LANSHARE_FILETRANSFER_H