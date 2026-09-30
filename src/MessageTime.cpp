#include "MessageTime.h"
#include "Device.h"
#include <QMovie>
#include <QLabel>
#include <QTextItem>
#include <utility>
#include <TimeTools.h>

MessageTime::MessageTime(const mlong time, const QString &bindId, QWidget *parent)
    : Message(parent) {
    this->time = time;
    this->bindId = bindId;
    initView();
}

void MessageTime::initView() {
    m_timeLabel = new QLabel(QString(TimeTools::getTimeMessage(time).c_str()), this);
    mainHLayout = new QHBoxLayout();
    mainHLayout->addStretch();
    mainHLayout->addWidget(m_timeLabel);
    mainHLayout->addStretch();
    mainHLayout->setContentsMargins(10, 10, 0, 10);
    setLayout(mainHLayout);
}

void MessageTime::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    Message::paintEvent(event);
}

void MessageTime::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
}
