#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <string>
#include <fstream>
#include <cstring>

std::string getExecutablePath() {
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        return std::string(buffer);
    }
    return "";
}

std::string getExecutableDir() {
    std::string path = getExecutablePath();
    if (!path.empty()) {
        char *path_cstr = new char[path.length() + 1];
        std::strcpy(path_cstr, path.c_str());
        std::string dir = std::string(dirname(path_cstr));
        delete[] path_cstr;
        return dir;
    }
    return "";
}

int main() {
    std::string exeDir = getExecutableDir();
    if (exeDir.empty()) {
        std::cerr << "Error getting executable directory" << std::endl;
        return 1;
    }

    std::string libsPath = exeDir + "/libs";
    std::string corePath = exeDir + "/core";

   
    // 设置工作目录
    if (chdir(exeDir.c_str()) != 0) {
        std::cerr << "Error changing directory to executable directory" << std::endl;
        return 1;
    }

    // 设置 LD_LIBRARY_PATH 环境变量
    if (setenv("LD_LIBRARY_PATH", libsPath.c_str(), 1) != 0) {
        std::cerr << "Error setting LD_LIBRARY_PATH" << std::endl;
        return 1;
    }

    // 使用 execv 执行 core 程序
    char *args[] = {const_cast<char*>(corePath.c_str()), nullptr};
    if (execv(args[0], args) == -1) {
        std::cerr << "Error executing core program" << std::endl;
        return 1;
    }

    return 0;
}
