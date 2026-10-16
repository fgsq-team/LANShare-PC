//
// Created by fgsq on 2023/10/9.
//
// 消息本地存储。改为只处理纯数据 ChatMessage，不再构造/持有 QWidget，
// 既解耦了 UI，也避免了在数据库调用路径上创建 Qt 控件带来的跨线程风险。
//

#ifndef LANSHARE_MESSAGEDB_H
#define LANSHARE_MESSAGEDB_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <list>
#include "ChatMessage.h"

class MessageDB {
public:
    static MessageDB &instance() {
        static MessageDB instance;
        return instance;
    }

private:
    MessageDB();

public:
    ~MessageDB();

    void addMessage(const ChatMessage &message);

    void updateMessage(const ChatMessage &message);

    void deleteMessage(const QString &uuid);

    void deleteAllMessage();

    /// 分页查询历史消息，返回纯数据（不含任何控件）。
    std::list<ChatMessage> queryList(int pageSize, int pageCount);

private:
    QSqlDatabase database;
    QSqlQuery *sql_query;
};

#endif //LANSHARE_MESSAGEDB_H
