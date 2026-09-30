//
// Created by fgsqme on 2023/12/20.
//

#ifndef LANSHARE_TOKENDBUTIL_H
#define LANSHARE_TOKENDBUTIL_H

#include <QtSql>
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

    void updateName(const QString &token, const QString &name) const;

    void setPass(const QString &token, int pass) const;

    void addToken(const QString &token, bool custom, int pass, const QString &ip, const QString &name) const;

    Token queryToken(const QString &token) const;

    Token queryCustomIp(const QString &customIp) const;

    QList<Token> queryList() const;

    void deleteToken(const QString &id) const;

private:
    void createTable() const;


private:
    QSqlDatabase database;
    QSqlQuery *sql_query;
};

#endif //LANSHARE_TOKENDBUTIL_H
