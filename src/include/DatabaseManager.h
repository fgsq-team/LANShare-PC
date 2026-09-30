//
// Created by fgsqme on 2024/5/6.
//
#include <QSqlDatabase>

#ifndef LANSHARE_DATABASEMANAGER_H
#define LANSHARE_DATABASEMANAGER_H

class DatabaseManager {
public:
    static QSqlDatabase& getDatabase() {
        static QSqlDatabase database;

        if (!database.isValid() || !database.isOpen()) {
            database = QSqlDatabase::addDatabase("QSQLITE", "default_connection");
            // 设置数据库路径等信息
        }

        return database;
    }
};

#endif //LANSHARE_DATABASEMANAGER_H
