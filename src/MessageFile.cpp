
#include "MessageFile.h"
#include "Device.h"
#include <QFontMetrics>
#include <QPaintEvent>
#include <QDateTime>
#include <QPainter>
#include <QMovie>
#include <QLabel>
#include <QDebug>
#include <QTextItem>
#include <QGraphicsTextItem>
#include <QTextBlock>
#include <QMargins>

MessageFile::MessageFile(QString message, QString userName, int devMode, bool left, bool isFile, QWidget *parent)
        : Message(parent) {
    this->message = message;
    this->userName = userName;
    this->devMode = devMode;
    this->left = left;
    this->m_isFile = isFile;
    initView();
}

void MessageFile::initView() {
    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setPixmap(QPixmap(":/img/ic_win.png").scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_avatarLabel->setFixedSize(40, 40);

    m_nicknameLabel = new QLabel(userName, this);
//    m_nicknameLabel->setStyleSheet("font-weight: bold;");

    m_textLabel = new CustomTextBrowser(this);
    m_textLabel->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_textLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textLabel->setMinimumWidth(150);
    m_textLabel->setMaximumWidth((int) (width() * 0.8));
    m_textLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textLabel->setStyleSheet("QTextBrowser { selection-background-color: dodgerblue; selection-color: white;background: rgba(0, 0, 0, 0);border: none; }");
    m_textLabel->setContentsMargins(0, 0, 0, 0);
    m_textLabel->setText(message);
    m_textLabel->document()->adjustSize();

    // 初始化文件消息相关控件
    m_fileIconLabel = new QLabel(this);
    m_fileSizeLabel = new QLabel(this);
    m_fileSizeLabel->setStyleSheet("color:#a8a8a8;");
    m_fileSizeLabel->setContentsMargins(4, 0, 0, 0);
    QPixmap fileIcon;
    if (isFile()) {
        fileIcon = QPixmap(":/img/rc_file_icon_file.png").scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        fileIcon = QPixmap(":/img/rc_dir_blue_icon.png").scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_fileIconLabel->setPixmap(fileIcon);
    m_fileIconLabel->setFixedSize(60, 60);
    m_fileSizeLabel->setWordWrap(true);
    m_fileSizeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setContentsMargins(4, 8, 0, 0);
    m_statusLabel->setStyleSheet("color:#a8a8a8;");
    m_statusLabel->setText("");
    QFont font = m_statusLabel->font();
    font.setPointSize(8);
    m_statusLabel->setFont(font);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    textLayout = new QVBoxLayout;
    if (!left) {
        m_nicknameLabel->setAlignment(Qt::AlignRight);  // 右对齐昵称
        m_textLabel->setAlignment(Qt::AlignLeft);  // 右对齐文本
    }
    textLayout->addWidget(m_textLabel);
    textLayout->setAlignment(m_textLabel, Qt::AlignRight);
    textLayout->addWidget(m_fileSizeLabel);
    textLayout->addWidget(m_statusLabel);
    textLayout->setSpacing(0);
    textLayout->setContentsMargins(5, 0, 10, 5);

    mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 5, 0, 5);
    auto *mainVLayout = new QVBoxLayout();
    mainVLayout->addWidget(m_nicknameLabel);

    mainHLayout = new QHBoxLayout();
    mainHLayout->addWidget(m_fileIconLabel);
    mainHLayout->addLayout(textLayout);
    mainHLayout->setContentsMargins(10, 10, 0, 10);

    mainVLayout->addLayout(mainHLayout);

    if (left) {
        mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignTop);  // 确保头像在顶部对齐
        mainLayout->addLayout(mainVLayout);
        mainLayout->addStretch();
    } else {
        mainLayout->addStretch();
        mainLayout->addLayout(mainVLayout);
        mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignTop);  // 确保头像在顶部对齐
    }

    setLayout(mainLayout);
    QTextDocument *textDocument = m_textLabel->document();
    docSize = textDocument->size().toSize();
    adjustTextWidth();
}

void MessageFile::adjustTextWidth() {
    int maxWidth = (int) (parentWidget()->width() * 0.8);
    if (maxWidth > 500) {
        maxWidth = 500;
    }
    QTextDocument *textDocument = m_textLabel->document();
    int idealWidth = std::max((int) (textDocument->idealWidth()), 110);
    // 设置文本宽度为实际内容的宽度
    textDocument->setTextWidth(idealWidth);
    QSizeF size = textDocument->size();
    int newHeight = size.height() + m_textLabel->mViewportMargins().top()
                    + m_textLabel->mViewportMargins().bottom() + 3;
//    qDebug() << "newHeight:" << newHeight;
    // 设置最小和最大宽度和高度以调整窗口大小
    m_textLabel->setFixedWidth(idealWidth);
    m_textLabel->setFixedHeight(newHeight);
    m_fileSizeLabel->setMaximumWidth(maxWidth);
    m_statusLabel->setMaximumWidth(maxWidth);
}

void MessageFile::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // 设置圆角矩形的背景颜色
//    qDebug() << "color:" << brush.color().red() << " " << brush.color().green() << " " << brush.color().blue() << " ";
    painter.setBrush(left ? config.chatBubblColorLeft : config.chatBubblColorRight);
    painter.setPen(Qt::NoPen);
    QPoint qPoint = m_fileIconLabel->pos();
    // 绘制圆角矩形
    QRect rect1 = QRect(
            qPoint.x() - 10,
            25,
            (int) mainHLayout->contentsRect().right() - qPoint.x() + 10,
            mainLayout->contentsRect().height() - 22);

    int radius = 5; // 圆角半径
    painter.drawRoundedRect(rect1, radius, radius);
    QPoint statusPos = m_statusLabel->pos();
    if (!recviced) {
        // ACC5D7
        QPen progressBkpen; //画笔
        progressBkpen.setColor({172, 197, 215});
        painter.setPen(progressBkpen);
        QRect progressBkRect(statusPos.x() + 5, statusPos.y() + 10, 100, 4);
        painter.setBrush(QBrush({172, 197, 215}));
        painter.drawRoundedRect(progressBkRect, 0, 0);
        QPen progresspen; //画笔
        progresspen.setColor({172, 197, 215});
        painter.setPen(progresspen);
        QRect progressRect(progressBkRect.x(), progressBkRect.top(), progress, 4);
        painter.setBrush(QBrush({00, 197, 215}));
        painter.drawRoundedRect(progressRect, 0, 0);
    }
//    qDebug() << "textLayout right:" << textLayout->contentsRect().right();
//    qDebug() << "qPoint.x:" << qPoint.x();
    Message::paintEvent(event);
}

int MessageFile::getProgress() const {
    return progress;
}

void MessageFile::setProgress(int progress) {
    MessageFile::progress = progress;
}

const QString &MessageFile::getFilePath() const {
    return filePath;
}

void MessageFile::setFilePath(const QString &filePath) {
    MessageFile::filePath = filePath;
}

long MessageFile::getFileSize() const {
    return fileSize;
}

void MessageFile::setFileSize(mlong fileSize) {
    MessageFile::fileSize = fileSize;
    m_fileSizeLabel->setText(Utils::computeSize(fileSize).c_str());
}

const QString &MessageFile::getFileSizeStr() const {
    return fileSizeStr;
}

void MessageFile::setFileSizeStr(const QString &fileSizeStr) {
    MessageFile::fileSizeStr = fileSizeStr;
}

bool MessageFile::isRecviced() const {
    return recviced;
}

void MessageFile::setRecviced(bool recviced) {
    MessageFile::recviced = recviced;
}

bool MessageFile::isCompleted() const {
    return completed;
}

void MessageFile::setCompleted(bool completed) {
    MessageFile::completed = completed;
    if (completed) {
        m_statusLabel->setText(left ? "接收成功" : "发送成功");
        m_statusLabel->setStyleSheet("QLabel { color : green; }");
    } else {
        m_statusLabel->setText(left ? "接收失败" : "发送失败");
        m_statusLabel->setStyleSheet("QLabel { color : red; }");
    }
}

bool MessageFile::isFile() const {
    return m_isFile;
}

void MessageFile::setIsFile(bool isFile) {
    MessageFile::m_isFile = isFile;
}

void MessageFile::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
}
