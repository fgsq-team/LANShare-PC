//
// Created by fgsqme on 2024/5/4.
//

#ifndef LANSHARE_MUTILS_H
#define LANSHARE_MUTILS_H

#define SEPARATORS "/"

#include <QString>
#include <QColor>
#include <QWidget>

class mUtils {
public:
    static QString createMultipleFolders(const QString &path);

    static void setFileAssociation(bool del);

    static QString generateRandomString(int length);

    static QColor parseColorFromStyleSheet(QString styleSheet, const QString &className, const QString &propertyName);

    static bool isDarkMode();

    static  QString decMessage(const QString &message, const QString &key);

    static QByteArray encMessage(const QString &message, const QString &key);
};


#endif //LANSHARE_MUTILS_H
