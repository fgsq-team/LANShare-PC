//
// Created by 16508 on 2025/8/30.
//

#ifndef LANSHARE_PROGRESSCALLBACK_H
#define LANSHARE_PROGRESSCALLBACK_H
#include "FileTransfer.h"

class ProgressCallback {
public:
    virtual ~ProgressCallback() = default;

public:
    virtual void onStart(FileTransfer *fileTransfer) = 0;

    virtual void onProgress(FileTransfer *fileTransfer, LFile *file) = 0;

    virtual void onFinish(FileTransfer *fileTransfer, LFile *file, bool isSuccess) = 0;
};
#endif //LANSHARE_PROGRESSCALLBACK_H
