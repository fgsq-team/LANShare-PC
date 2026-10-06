#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>
#include <QFileInfo>
#include <QFile>
#include <QDir>

class FileUtils {
public:
    /**
     * 检查路径是否存在（可以是文件或目录）
     * @param path 要检查的路径
     * @return 存在返回true，否则返回false
     */
    static bool exists(const QString& path) {
        return QFileInfo::exists(path);
    }
    
    /**
     * 检查文件是否存在
     * @param filePath 文件路径
     * @return 文件存在且为文件类型返回true，否则返回false
     */
    static bool fileExists(const QString& filePath) {
        QFileInfo fileInfo(filePath);
        return fileInfo.exists() && fileInfo.isFile();
    }
    
    /**
     * 检查目录是否存在
     * @param dirPath 目录路径
     * @return 目录存在且为目录类型返回true，否则返回false
     */
    static bool dirExists(const QString& dirPath) {
        QFileInfo fileInfo(dirPath);
        return fileInfo.exists() && fileInfo.isDir();
    }
};

#endif // FILEUTILS_H
