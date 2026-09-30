//
// Created by fgsqme on 2023/12/20.
//

#ifndef LANSHARE_TOKEN_H
#define LANSHARE_TOKEN_H

#include <QString>

class Token {
public:
    Token();

    Token(bool null);

    QString getToken() const;

    void setToken(const QString &token);

    QString getIp() const;

    void setIp(const QString &ip);

    bool isCustom() const;

    void setCustom(bool custom);

    bool isNull() const;

    void setNull(bool null);

    void setPass(int pass);

    int getPass() const;

    void setName(const QString &name);

    QString getName() const;

private:
    QString token;
    QString ip;
    QString name;
    bool custom;
    int pass;
    bool null = false;
};

#endif //LANSHARE_TOKEN_H
