#include "MessageFile.h"
#include "FileUtils.h"
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
#include <QSvgRenderer>
#include <QFile>
#include <QTextStream>

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
    if (!left) {
        m_nicknameLabel->setStyleSheet("color: #FFFFFF;");
    }

    m_textLabel = new CustomTextBrowser(this);
    m_textLabel->setObjectName(left ? "chatTextLeft" : "chatTextRight");
    m_textLabel->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_textLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textLabel->setMinimumWidth(150);
    m_textLabel->setMaximumWidth((int) (width() * 0.8));
    m_textLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // selection 颜色和文字颜色由 QSS 控制
    m_textLabel->setStyleSheet(
        "QTextBrowser { background: rgba(0, 0, 0, 0); border: none; }");
    m_textLabel->setContentsMargins(0, 0, 0, 0);
    m_textLabel->setText(message);
    m_textLabel->document()->adjustSize();

    // 初始化文件消息相关控件
    m_fileIconLabel = new QLabel(this);
    m_fileSizeLabel = new QLabel(this);
    m_fileSizeLabel->setContentsMargins(4, 0, 0, 0);
    m_fileSizeLabel->setStyleSheet(QString("color:%1;").arg(!left ? "#B0B6BF" : Config::instance().textColor.name()));
    QPixmap fileIcon;
    if (isFile()) {
        fileIcon = loadSvgPixmap(":/img/svgs/rc_file_icon_file.svg", 60);
    } else {
        fileIcon = loadSvgPixmap(":/img/svgs/rc_dir_blue_icon.svg", 60);
    }
    m_fileIconLabel->setPixmap(fileIcon);
    m_fileIconLabel->setFixedSize(60, 60);
    m_fileSizeLabel->setWordWrap(true);
    m_fileSizeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setContentsMargins(4, 8, 0, 0);
    m_statusLabel->setStyleSheet(QString("color:%1;").arg(!left ? "#FFFFFF" : Config::instance().textColor.name()));
    m_statusLabel->setText("");
    QFont font = m_statusLabel->font();
    font.setPointSize(8);
    m_statusLabel->setFont(font);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    textLayout = new QVBoxLayout;
    if (!left) {
        m_nicknameLabel->setAlignment(Qt::AlignRight); // 右对齐昵称
        m_textLabel->setAlignment(Qt::AlignLeft); // 右对齐文本
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
        mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignTop); // 确保头像在顶部对齐
        mainLayout->addLayout(mainVLayout);
        mainLayout->addStretch();
    } else {
        mainLayout->addStretch();
        mainLayout->addLayout(mainVLayout);
        mainLayout->addWidget(m_avatarLabel, 0, Qt::AlignTop); // 确保头像在顶部对齐
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
    painter.setBrush(left ? Config::instance().chatBubblColorLeft : Config::instance().chatBubblColorRight);
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
        QColor progressBg = Config::instance().selectionBgColor;
        QPen progressBkpen; //画笔
        progressBkpen.setColor(progressBg);
        painter.setPen(progressBkpen);
        QRect progressBkRect(statusPos.x() + 5, statusPos.y() + 10, 100, 4);
        painter.setBrush(QBrush(progressBg));
        painter.drawRoundedRect(progressBkRect, 0, 0);
        QPen progresspen; //画笔
        progresspen.setColor(Config::instance().primaryColor);
        painter.setPen(progresspen);
        QRect progressRect(progressBkRect.x(), progressBkRect.top(), progress, 4);
        painter.setBrush(QBrush(Config::instance().primaryColor));
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

mlong MessageFile::getFileSize() const {
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
        if (!isLeft() || FileUtils::exists(filePath)) {
            m_statusLabel->setText(left ? tr("接收成功") : tr("发送成功"));
            m_statusLabel->setStyleSheet(QString("QLabel { color : %1; }").arg(Config::instance().successColor.name()));
        } else {
            m_statusLabel->setText(tr("文件已被删除"));
            m_statusLabel->setStyleSheet(QString("QLabel { color : %1; }").arg(Config::instance().errorColor.name()));
        }
    } else {
        m_statusLabel->setText(left ? tr("接收失败") : tr("发送失败"));
        m_statusLabel->setStyleSheet(QString("QLabel { color : %1; }").arg(Config::instance().errorColor.name()));
    }
}

bool MessageFile::isFile() const {
    return m_isFile;
}

void MessageFile::setIsFile(bool isFile) {
    MessageFile::m_isFile = isFile;
}

void MessageFile::changeEvent(QEvent *event) {
    if (event->type() == QEvent::StyleChange) {
        // 主题切换时更新文件图标
        updateFileIcon();
        // 主题切换时更新文件大小和状态标签颜色
        if (m_fileSizeLabel) {
            m_fileSizeLabel->setStyleSheet(QString("color:%1;").arg(!left ? "#B0B6BF" : Config::instance().textColor.name()));
        }
        if (m_statusLabel) {
            if (completed) {
                if (!isLeft() || FileUtils::exists(filePath)) {
                    m_statusLabel->setStyleSheet(
                        QString("QLabel { color : %1; }").arg(Config::instance().successColor.name()));
                } else {
                    m_statusLabel->setStyleSheet(
                        QString("QLabel { color : %1; }").arg(Config::instance().errorColor.name()));
                }
            } else if (!completed) {
                m_statusLabel->setStyleSheet(
                    QString("QLabel { color : %1; }").arg(Config::instance().errorColor.name()));
            } else {
                m_statusLabel->setStyleSheet(QString("color:%1;").arg(Config::instance().textColor.name()));
            }
        }
    }
    QWidget::changeEvent(event);
}

void MessageFile::updateFileIcon() {
    QPixmap fileIcon;
    if (isFile()) {
        fileIcon = loadSvgPixmap(":/img/svgs/rc_file_icon_file.svg", 60);
    } else {
        fileIcon = loadSvgPixmap(":/img/svgs/rc_dir_blue_icon.svg", 60);
    }
    m_fileIconLabel->setPixmap(fileIcon);
}

QPixmap MessageFile::loadSvgPixmap(const QString &path, int size) const {
    // 读取 SVG 内容并替换颜色占位符
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QPixmap(size, size);
    }
    QString svgContent = QTextStream(&file).readAll();
    file.close();

    // 左气泡：背景=主题色，图标=白色；右气泡：背景=白色，图标=主题色
    QColor primaryColor = Config::instance().primaryColor;
    if (left) {
        svgContent.replace("__BG__", primaryColor.name());
        svgContent.replace("__ICON__", "#FFFFFF");
    } else {
        svgContent.replace("__BG__", "#FFFFFF");
        svgContent.replace("__ICON__", primaryColor.name());
    }
    QSvgRenderer renderer(svgContent.toUtf8());
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRect(0, 0, size, size));
    return pixmap;
}
