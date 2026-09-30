//
// Created by user on 2024/5/31.
//

#ifndef LANSHARE_EDITTEXTEVENTFILTER_H
#define LANSHARE_EDITTEXTEVENTFILTER_H

#include <QObject>
#include <QEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QMenu>
#include <QTextEdit>

class EditTextEventFilter : public QObject {
Q_OBJECT

private:
    static EditTextEventFilter *editTextEventFilterInstance;  // 静态单例指针
    EditTextEventFilter() {};

public:
    static EditTextEventFilter *getInstance();

    bool eventFilter(QObject *obj, QEvent *event) override;

    ~EditTextEventFilter() override;
};

#endif //LANSHARE_EDITTEXTEVENTFILTER_H
