//
// Created by fgsq on 2023/10/9.
//

#ifndef LANSHARE_MESSAGEDB_H
#define LANSHARE_MESSAGEDB_H


#include <QSqlDatabase>
#include <list>
#include "Message.h"


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

    void addMessage(Message *message);

    void updateMessage(Message *message);

    void deleteMessage(const QString &uuid);

    void deleteAllMessage();

    std::list<Message *> queryList(int pageSize, int pageCount, QWidget *parent);


private:
    QSqlDatabase database;
    QSqlQuery *sql_query;
};


#endif //LANSHARE_MESSAGEDB_H
