//
// Created by fgsqme on 2023/12/20.
//

#ifndef LANSHARE_TOKENDBUTIL_H
#define LANSHARE_TOKENDBUTIL_H

#include <QtSql>
#include <QThread>
#include <mutex>
#include "Token.h"

class TokenDBUtil {
public:
    static TokenDBUtil &instance() {
        static TokenDBUtil instance;
        return instance;
    }
public:
    TokenDBUtil();

    ~TokenDBUtil();

    void addToken(const QString &token, bool custom, const QString &ip);

    QString queryToken(const QString &token);

    Token queryCustomIp(const QString &customIp);

    QList<Token> queryList();

    void deleteToken(const QString &id);

private:
    void createTable();

    QSqlDatabase getThreadLocalDb();

private:
    QSqlDatabase database;
    QSqlQuery *sql_query;
    std::mutex dbMutex;
    QThread *ownerThread;
};

#endif //LANSHARE_TOKENDBUTIL_H
