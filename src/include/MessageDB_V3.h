//
// Created by fgsq on 2023/10/9.
//

#ifndef LANSHARE_MESSAGEDB_H
#define LANSHARE_MESSAGEDB_H


#include <QSqlDatabase>
#include <list>
#include "Message.h"


class MessageDB_V3 {
public:
    static MessageDB_V3 &instance() {
        static MessageDB_V3 instance;
        return instance;
    }

private:
    MessageDB_V3();

public:
    ~MessageDB_V3();

    void addMessage(Message *message) const;

    void updateMessage(Message *message) const;

    void deleteMessage(const QString &uuid) const;

    void deleteAllMessage();

    mlong getLastMessageTime() const;

    std::list<Message *> queryList(int pageSize, int pageCount, QWidget *parent) const;


private:
    QSqlDatabase database;
    QSqlQuery *sql_query;
};


#endif //LANSHARE_MESSAGEDB_H
