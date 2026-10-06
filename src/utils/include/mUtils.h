//
// Created by fgsqme on 2024/5/4.
//

#ifndef LANSHARE_MUTILS_H
#define LANSHARE_MUTILS_H

#define SEPARATORS "/"

#include <QString>
#include <QColor>
#include <QWidget>
#include <QFileInfo>

#include "Type.h"

class mUtils {
public:
    static QString createMultipleFolders(const QString &path);

    static void setFileAssociation(bool del);

    static QColor parseColorFromStyleSheet(QString styleSheet, const QString &className, const QString &propertyName);

    static bool isDarkMode();

    static QString decMessage(const QString &message, const QString &key);

    static QByteArray encMessage(const QString &message, const QString &key);

    static QString avoidDuplication(const QFileInfo &outFile);

    static void createEmptyFileWithSaveFile(const QString &filename);

    static void encData(mbyte *buffer, int len, int off, mlong index);

    static void decData(mbyte *buffer, int len, int off, mlong index);
};


#endif //LANSHARE_MUTILS_H
