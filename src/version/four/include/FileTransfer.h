//
// Created by 16508 on 2025/8/30.
//

#ifndef LANSHARE_FILETRANSFER_H
#define LANSHARE_FILETRANSFER_H
#include "Device.h"
#include <vector>

#include "MessageFile.h"

class FileTransfer {
public:
    Device fromDevice;
    Device toDevice;
    int type;
    QString groupId;
    std::vector<LFile*> files;
};


#endif //LANSHARE_FILETRANSFER_H