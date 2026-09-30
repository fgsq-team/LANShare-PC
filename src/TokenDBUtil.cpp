#include "TokenDBUtil.h"
#include "Config.h"
#include "StringLockManager.h"

TokenDBUtil::TokenDBUtil() {
    if (QSqlDatabase::contains("token_list_v2")) {
        database = QSqlDatabase::database("token_list_v2");
    } else {
        database = QSqlDatabase::addDatabase("QSQLITE", "token_list_v2");
        database.setDatabaseName(config.lanshareWorkDirPath + "/token_list_v2.db");
        database.setUserName("lanshare");
        database.setPassword("uacvbtyaw");
    }
    if (!database.open()) {
        qDebug() << "打开数据库文件失败";
        return;
    }
    sql_query = new QSqlQuery(database);
    createTable();
}

TokenDBUtil::~TokenDBUtil() {
    if (database.isOpen()) {
        database.close();
    }
    delete sql_query;
}

void TokenDBUtil::updateName(const QString &token, const QString &name) const {
    if (!database.isOpen()) {
        return;
    }
    sql_query->prepare("UPDATE token SET name = :name WHERE id = :id AND isdel = 0");
    sql_query->bindValue(":name", name);
    sql_query->bindValue(":id", token);
    if (!sql_query->exec()) {
        qCritical() << "Failed to update name for token:" << token << sql_query->lastError().text();
    }
}

void TokenDBUtil::setPass(const QString &token, int pass) const {
    if (!database.isOpen()) {
        return;
    }
    sql_query->prepare("UPDATE token SET pass = :pass WHERE id = :id AND isdel = 0");
    sql_query->bindValue(":pass", pass);
    sql_query->bindValue(":id", token);
    if (!sql_query->exec()) {
        qCritical() << "Failed to update pass for token:" << token << sql_query->lastError().text();
    }
}

void TokenDBUtil::addToken(const QString &token, bool custom, int pass, const QString &ip, const QString &name) const {
    if (!database.isOpen()) {
        return;
    }
    sql_query->prepare(
        "INSERT INTO token (id, custom, pass , ip,  name, isdel) VALUES (:id, :custom, :pass, :ip, :name, 0)");
    sql_query->bindValue(":id", token);
    sql_query->bindValue(":custom", custom ? 1 : 0);
    sql_query->bindValue(":pass", pass);
    sql_query->bindValue(":ip", ip);
    sql_query->bindValue(":name", name);
    if (!sql_query->exec()) {
        qCritical() << "Failed to add token:" << token << sql_query->lastError().text();
    }
}

Token TokenDBUtil::queryToken(const QString &token) const {
    if (!database.isOpen()) {
        return {true};
    }
    std::mutex &queryTokenLock = StringLockManager::getStringLock("queryToken");
    std::lock_guard lock(queryTokenLock); // 自动锁定 mtx
    sql_query->prepare("SELECT id,ip,name,custom,pass FROM token WHERE id = :token AND isdel = 0");
    sql_query->bindValue(":token", token);
    if (sql_query->exec() && sql_query->next()) {
        Token t;
        t.setToken(sql_query->value(0).toString());
        t.setIp(sql_query->value(1).toString());
        t.setName(sql_query->value(2).toString());
        t.setCustom(sql_query->value(3).toInt() == 1);
        t.setPass(sql_query->value(4).toInt());
        return t;
    }
    return {true};
}

Token TokenDBUtil::queryCustomIp(const QString &customIp) const {
    if (!database.isOpen()) {
        return {true};
    }
    sql_query->prepare(
        "SELECT id, ip, name, custom, pass FROM token WHERE ip = :customIp AND custom = 1 AND isdel = 0");
    sql_query->bindValue(":customIp", customIp);
    if (sql_query->exec() && sql_query->next()) {
        Token token;
        token.setToken(sql_query->value(0).toString());
        token.setIp(sql_query->value(1).toString());
        token.setName(sql_query->value(2).toString());
        token.setCustom(sql_query->value(3).toInt() == 1);
        token.setPass(sql_query->value(4).toInt());
        return token;
    }
    return {true};
}

QList<Token> TokenDBUtil::queryList() const {
    if (!database.isOpen()) {
        return {};
    }
    QList<Token> list;
    sql_query->exec("SELECT id, ip, custom, pass FROM token WHERE isdel = 0");
    while (sql_query->next()) {
        Token token;
        token.setToken(sql_query->value(0).toString());
        token.setIp(sql_query->value(1).toString());
        token.setName(sql_query->value(2).toString());
        token.setCustom(sql_query->value(3).toInt() == 1);
        token.setPass(sql_query->value(4).toInt());
        list.append(token);
    }

    return list;
}

void TokenDBUtil::deleteToken(const QString &id) const {
    if (!database.isOpen()) {
        return;
    }
    sql_query->prepare("DELETE FROM token WHERE id = :id");
    sql_query->bindValue(":id", id);
    if (!sql_query->exec()) {
        qCritical() << "Failed to delete token:" << id << sql_query->lastError().text();
    }
}

void TokenDBUtil::createTable() const {
    if (!database.isOpen()) {
        return;
    }
    if (!sql_query->exec(
        "CREATE TABLE IF NOT EXISTS token (id VARCHAR(32) PRIMARY KEY, ip VARCHAR(16), name VARCHAR(16), custom INTEGER, pass INTEGER, isdel INTEGER)")) {
        qCritical() << "Failed to create table:" << sql_query->lastError().text();
    }
}
