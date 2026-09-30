//
// Created by user on 2025/1/20.
//

#ifndef MESSAGETIME_H
#define MESSAGETIME_H


#include <QWidget>
#include <QTextEdit>
#include <QProgressBar>
#include "Message.h"

class QPaintEvent;

class QPainter;

class QLabel;

class QMovie;


class MessageTime : public Message {
    Q_OBJECT

public:
    MessageTime(mlong time, const QString &pid, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

    void changeEvent(QEvent *event) override;

private:
    mlong time;
    QString bindId;
    QLabel *m_timeLabel{};

    QVBoxLayout *textLayout{};
    QHBoxLayout *mainHLayout{};
    QHBoxLayout *mainLayout{};

    QSize docSize{};

public:
    QString getBindId() const {
        return bindId;
    }

    mlong getTime() {
        return time;
    }

private:
    void initView();
};


#endif //MESSAGETIME_H
