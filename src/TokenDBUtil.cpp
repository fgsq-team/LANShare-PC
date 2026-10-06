#include "TokenDBUtil.h"
#include "Config.hpp"
#include "StringLockManager.h"
#include <QThread>

TokenDBUtil::TokenDBUtil() : ownerThread(QThread::currentThread()) {
    if (QSqlDatabase::contains("token_list_new")) {
        database = QSqlDatabase::database("token_list_new");
    } else {
        database = QSqlDatabase::addDatabase("QSQLITE", "token_list_new");
        database.setDatabaseName(Config::instance().lanshareWorkDirPath + "/token_list_new.db");
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

QSqlDatabase TokenDBUtil::getThreadLocalDb() {
    // 如果在主线程（创建连接的线程），直接返回原始连接
    if (QThread::currentThread() == ownerThread) {
        return database;
    }
    // 在工作线程中，创建线程独立的连接
    QString connName = "token_list_thread_" + QString::number(reinterpret_cast<quintptr>(QThread::currentThread()));
    if (!QSqlDatabase::contains(connName)) {
        auto db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(Config::instance().lanshareWorkDirPath + "/token_list_new.db");
        db.setUserName("lanshare");
        db.setPassword("uacvbtyaw");
        if (!db.open()) {
            qDebug() << "线程数据库打开失败:" << connName;
        }
    }
    return QSqlDatabase::database(connName);
}

void TokenDBUtil::addToken(const QString &token, bool custom, const QString &ip) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sql_query->prepare("INSERT INTO token (id, custom, ip, isdel) VALUES (:id, :custom, :ip, 0)");
    sql_query->bindValue(":id", token);
    sql_query->bindValue(":custom", custom ? 1 : 0);
    sql_query->bindValue(":ip", ip);
    if (!sql_query->exec()) {
        qCritical() << "Failed to add token:" << token << sql_query->lastError().text();
    }
}

QString TokenDBUtil::queryToken(const QString &token) {
    std::lock_guard<std::mutex> lock(dbMutex);
    QSqlDatabase db = getThreadLocalDb();
    QSqlQuery query(db);
    query.prepare("SELECT id FROM token WHERE id = :token AND isdel = 0");
    query.bindValue(":token", token);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return {};
}

Token TokenDBUtil::queryCustomIp(const QString &customIp) {
    std::lock_guard<std::mutex> lock(dbMutex);
    QSqlDatabase db = getThreadLocalDb();
    QSqlQuery query(db);
    query.prepare("SELECT id, ip, custom FROM token WHERE ip = :customIp AND custom = 1 AND isdel = 0");
    query.bindValue(":customIp", customIp);
    if (query.exec() && query.next()) {
        Token token;
        token.setToken(query.value(0).toString());
        token.setIp(query.value(1).toString());
        token.setCustom(query.value(2).toInt() == 1);
        return token;
    }

    return {true};
}

QList<Token> TokenDBUtil::queryList() {
    std::lock_guard<std::mutex> lock(dbMutex);
    QList<Token> list;
    QSqlDatabase db = getThreadLocalDb();
    QSqlQuery query(db);
    query.exec("SELECT id, ip, custom FROM token WHERE isdel = 0");
    while (query.next()) {
        Token token;
        token.setToken(query.value(0).toString());
        token.setIp(query.value(1).toString());
        token.setCustom(query.value(2).toInt() == 1);
        list.append(token);
    }

    return list;
}

void TokenDBUtil::deleteToken(const QString &id) {
    std::lock_guard<std::mutex> lock(dbMutex);
    sql_query->prepare("DELETE FROM token WHERE id = :id");
    sql_query->bindValue(":id", id);
    if (!sql_query->exec()) {
        qCritical() << "Failed to delete token:" << id << sql_query->lastError().text();
    }
}

void TokenDBUtil::createTable() {
    if (!sql_query->exec(
            "CREATE TABLE IF NOT EXISTS token (id VARCHAR(32) PRIMARY KEY, ip VARCHAR(16), custom INTEGER, isdel INTEGER)")) {
        qCritical() << "Failed to create table:" << sql_query->lastError().text();
    }
}
