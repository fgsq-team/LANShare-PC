#include <string>
#include <vector>
#include <cstring>

#include "Type.h"

struct FileEntry {
    std::string path;
    mlong beginOffset;
    mlong endOffset;
    mlong fileSize;
};

static const unsigned char resource_data[] = {};
static const std::vector<FileEntry> fileEntrys = {};

int openData(const std::string &resName) {
    for (int i = 0; i < fileEntrys.size(); ++i) {
        if (resName == fileEntrys[i].path) {
            return i;
        }
    }
    return -1;
}

int readData(int fd, char *buff, mlong offset, int len) {
    if (fd == -1 || fd >= fileEntrys.size()) {
        return -1;
    }
    FileEntry fileEntr = fileEntrys[fd];
    if (offset > fileEntr.fileSize) {
        return -1;
    }
    int canReadSize = std::min(len, (int) (fileEntr.fileSize - offset));
    memcpy(buff, resource_data + (fileEntr.beginOffset + offset), canReadSize);
    return canReadSize;
}

long getFileSize(int fd) {
    if (fd == -1 || fd >= fileEntrys.size()) {
        return -1;
    }
    FileEntry fileEntr = fileEntrys[fd];
    return fileEntr.fileSize;
}
