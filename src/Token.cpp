//
// Created by fgsqme on 2023/12/20.
//

#include "Token.h"

Token::Token() : custom(false) {}

Token::Token(bool null) : null(null) {}

QString Token::getToken() const {
    return token;
}

void Token::setToken(const QString &token) {
    this->token = token;
}

QString Token::getIp() const {
    return ip;
}

void Token::setIp(const QString &ip) {
    this->ip = ip;
}

bool Token::isCustom() const {
    return custom;
}

void Token::setCustom(bool custom) {
    this->custom = custom;
}

bool Token::isNull() const {
    return null;
}

void Token::setNull(bool null) {
    Token::null = null;
}

void Token::setPass(int pass) {
    this->pass = pass;
}

int Token::getPass() const {
    return pass;
}

void Token::setName(const QString &name) {
    this->name = name;
}

QString Token::getName() const {
    return name;
}
