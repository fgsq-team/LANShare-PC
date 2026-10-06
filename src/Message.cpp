
#include "Message.h"
#include "Device.h"
#include <QPaintEvent>
#include <QDateTime>
#include <QPainter>
#include <QMovie>
#include <QLabel>
#include <QTextItem>
#include <QGraphicsTextItem>
#include <QVBoxLayout>
#include <QtGlobal>
#include <QTextBlock>
#include <utility>

Message::Message(QWidget *parent)
        : QWidget(parent) {
//    setStyleSheet("background-color: rgb(247, 247, 247);");
}

Message::Message(QString message, QString userName, int devMode, bool left, QWidget *parent)
        : message(std::move(message)), devMode(devMode), left(left), userName(std::move(userName)), QWidget(parent) {
//    setStyleSheet("background-color: rgb(247, 247, 247);");
    initView();
}

void Message::initView() {
    m_avatarLabel = new QLabel(this);
    QPixmap pixmap;
    if (devMode == Device::L_ANDROID) {
        pixmap = QPixmap(":/img/ic_phone.png");
    } else if (devMode == Device::L_WIN) {
        pixmap = QPixmap(":/img/ic_win.png");
    } else {
        pixmap = QPixmap(":/img/ic_launcher_32.png");
    }
    m_avatarLabel->setPixmap(pixmap.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_avatarLabel->setFixedSize(40, 40);
    m_nicknameLabel = new QLabel(userName, this);
//    m_nicknameLabel->setStyleSheet("font-weight: bold;");
    m_textLabel = new CustomTextBrowser(this);
    m_textLabel->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_textLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textLabel->setText(message);
    m_textLabel->document()->adjustSize();
    // 设置气泡背景颜色和边框
    QString bubbleStyle = QString(
            "QTextBrowser { selection-background-color: %1; selection-color: %2;background-color: %3; border-radius: 10px; padding: 10px;border: none;}")
            .arg(Config::instance().selectionBgColor.name(), Config::instance().selectionTextColor.name(),
                 left
                 ? Config::instance().chatBubblColorLeft.color().name()
                 : Config::instance().chatBubblColorRight.color().name());
    m_textLabel->setStyleSheet(bubbleStyle);
    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 5, 0, 5);
    if (!left) {
        m_nicknameLabel->setAlignment(Qt::AlignRight);  // 右对齐昵称
        m_textLabel->setAlignment(Qt::AlignLeft);  // 右对齐文本
    }
    textLayout->addWidget(m_nicknameLabel);
    textLayout->addWidget(m_textLabel);
    textLayout->setAlignment(m_textLabel, Qt::AlignLeft);  // 确保头像在顶部对齐
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 5, 0, 5);
    if (left) {
        mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignTop);  // 确保头像在顶部对齐
        mainLayout->addLayout(textLayout);
        mainLayout->addStretch();
    } else {
        mainLayout->addStretch();
        mainLayout->addLayout(textLayout);
        mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignTop);  // 确保头像在顶部对齐
    }
    setLayout(mainLayout);
    adjustTextWidth();  // 初始化时调整文本宽度
}

void Message::adjustTextWidth() {
    int maxWidth = (int) (parentWidget()->width() * 0.8);
    if (maxWidth > 500) {
        maxWidth = 500;
    }
    m_textLabel->setFixedWidth(maxWidth);
    QTextDocument *textDocument = m_textLabel->document();
    QSize docSize = textDocument->size().toSize();
    m_textLabel->setFixedHeight(docSize.height() + 20);
    m_textLabel->setFixedWidth((int) (textDocument->idealWidth() + 21));
}

QSize Message::sizeHint() const {
    return QWidget::sizeHint();
}

void Message::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // 动态变化消息气泡宽高(性能不太行)
//    adjustTextWidth();
}

void Message::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
}

bool Message::isLeft() const {
    return left;
}

const QString &Message::getDeviceName() const {
    return userName;
}

const QString &Message::getUuid() const {
    return uuid;
}

void Message::setUuid(const QString &uuid) {
    Message::uuid = uuid;
}

const QString &Message::getMessage() const {
    return message;
}

const QString &Message::getUserName() const {
    return userName;
}

LFile *Message::getFile() const {
    return file;
}

void Message::setFile(LFile *file) {
    Message::file = file;
}

int Message::getDevMode() const {
    return devMode;
}

void Message::setDevMode(int devMode) {
    Message::devMode = devMode;
}

void Message::changeEvent(QEvent *event) {
    if (event->type() == QEvent::StyleChange) {
        QString bubbleStyle = QString(
                "QTextBrowser { selection-background-color: %1; selection-color: %2;background-color: %3; border-radius: 10px; padding: 10px;border: none;}")
                .arg(Config::instance().selectionBgColor.name(), Config::instance().selectionTextColor.name(),
                     left
                     ? Config::instance().chatBubblColorLeft.color().name()
                     : Config::instance().chatBubblColorRight.color().name());
        m_textLabel->setStyleSheet(bubbleStyle);
    }
    QWidget::changeEvent(event);
}

int Message::getDataVersion() const {
    return dataVersion;
}

void Message::setDataVersion(int dataVersion) {
    Message::dataVersion = dataVersion;
}
