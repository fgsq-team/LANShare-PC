#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <filesystem>
#include <cstring>

#include "Type.h"
//#include "resource.h"

extern int openData(const std::string &resName);

extern int readData(int fd, char *buff, long offset, int len);

namespace fs = std::filesystem;



struct FileEntry {
    std::string path;
    mlong beginOffset;
    mlong endOffset;
    mlong fileSize;
};

std::vector<FileEntry> entrys;
mlong rindex = 0;
mlong lastIndex = 0;
mlong first = true;

std::string fileToHex(int subNameLength, const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return "";
    }
    std::ostringstream oss;
    unsigned char c;
    while (file >> std::noskipws >> c) {
        if (!first) oss << ",";
        first = false;
        oss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c);
        rindex++;
    }
    entrys.push_back({filename.substr(subNameLength), lastIndex, rindex, rindex - lastIndex});
    lastIndex = rindex;
    return oss.str();
}

void processFilesInFolder(int subNameLength, const std::string &folderPath, std::ofstream &outputFile) {
    for (const auto &entry: fs::directory_iterator(folderPath)) {
        if (fs::is_regular_file(entry.path())) {
            outputFile << "// " << entry.path().generic_string().substr(subNameLength) << std::endl;
            outputFile << fileToHex(subNameLength, entry.path().generic_string()) << std::endl;
        } else if (fs::is_directory(entry.path())) {
            processFilesInFolder(subNameLength, entry.path().generic_string(), outputFile);
        }
    }
}


int main() {
    std::string folderPath = R"(D:\Project\LANShare-PC\web)";
    std::ofstream outputFile("D:\\Project\\LANShare-PC\\build-tools\\res2cpp\\resource1.cpp");
    if (!outputFile) {
        std::cerr << "Failed to create resource.cpp" << std::endl;
        return 1;
    }
    outputFile << "#include <string>" << std::endl;
    outputFile << "#include <vector>" << std::endl;
    outputFile << std::endl;
    outputFile << "struct FileEntry {" << std::endl;
    outputFile << "    std::string path;" << std::endl;
    outputFile << "    long beginOffset;" << std::endl;
    outputFile << "    long endOffset;" << std::endl;
    outputFile << "    long fileSize;" << std::endl;

    outputFile << "};" << std::endl;
    outputFile << std::endl;

    std::ostringstream oss;
    outputFile << "static const unsigned char resource_data[] = {" << std::endl;
    fs::path filePath = folderPath;
    // 获取上级文件夹路径
    fs::path parentPath = filePath.parent_path();
    std::string str = parentPath.generic_string();
    processFilesInFolder(str.length() + 1, folderPath, outputFile);
    outputFile << "};" << std::endl;
    outputFile << "static const std::vector<FileEntry> fileEntrys = {" << std::endl;
    for (const auto &entry: entrys) {
        outputFile << "    {\"" << entry.path << "\"," << entry.beginOffset << "," << entry.endOffset << ","
                   << entry.fileSize << "},"
                   << std::endl;
    }
    outputFile << "};" << std::endl;
    outputFile << std::endl;
    outputFile << "int openData(const std::string &resName) {" << std::endl;
    outputFile << "    for (int i = 0; i < fileEntrys.size(); ++i) {" << std::endl;
    outputFile << "        if (resName == fileEntrys[i].path) {" << std::endl;
    outputFile << "            return i;" << std::endl;
    outputFile << "        }" << std::endl;
    outputFile << "    }" << std::endl;
    outputFile << "    return -1;" << std::endl;
    outputFile << "}" << std::endl;
    outputFile << std::endl;
    outputFile << "int readData(int fd, char *buff, long offset, int len) {" << std::endl;
    outputFile << "    if (fd == -1 || fd >= fileEntrys.size()) {" << std::endl;
    outputFile << "        return -1;" << std::endl;
    outputFile << "    }" << std::endl;
    outputFile << "    FileEntry fileEntr = fileEntrys[fd];" << std::endl;
    outputFile << "    if (offset > fileEntr.fileSize) {" << std::endl;
    outputFile << "        return -1;" << std::endl;
    outputFile << "    }" << std::endl;
    outputFile << "    int canReadSize = std::min(len, (int) (fileEntr.fileSize - offset));" << std::endl;
    outputFile << "    memcpy(buff, resource_data + (fileEntr.beginOffset + offset), canReadSize);" << std::endl;
    outputFile << "    return canReadSize;" << std::endl;
    outputFile << "}" << std::endl;
    outputFile << std::endl;
    outputFile << "long getFileSize(int fd) {" << std::endl;
    outputFile << "    if (fd == -1 || fd >= fileEntrys.size()) {" << std::endl;
    outputFile << "        return -1;" << std::endl;
    outputFile << "    }" << std::endl;
    outputFile << "    FileEntry fileEntr = fileEntrys[fd];" << std::endl;
    outputFile << "    return fileEntr.fileSize;" << std::endl;
    outputFile << "}" << std::endl;

    // int fd = openData("web/css/animate.min.css");
    // char ten;
    // readData(fd, &ten, 0, 1);
    // std::cout << "fd:" << fd << std::endl;
    // std::cout << "ten:" << (int) ten << std::endl;
    return 0;
}

