//
// ChatItemWidget 渲染层实现。
//
#include "ChatItemWidget.h"
#include "ChatTheme.h"
#include "Config.hpp"
#include "Device.h"
#include "LFile.h"
#include "FileUtils.h"
#include "ThumbnailUtil.h"

#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QMenu>
#include <QClipboard>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QSvgRenderer>
#include <QtMath>
#include <QDesktopServices>
#include <QUrl>
#include <QTextDocument>
#include <QTimer>
#include <QFont>
#include <QFocusEvent>
#include <QContextMenuEvent>
#include <QDebug>

namespace {
/// 读取 SVG 内容并按主题替换颜色占位符，渲染成 pixmap。
/// 复用旧 MessageFile::loadSvgPixmap 的配色约定：
///   左侧气泡：底=主题色、图标=白色；右侧气泡：底=白色、图标=主题色。
QPixmap recolorSvgPixmap(const QString &path, int size, ChatDirection dir) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QPixmap(size, size);
    }
    QString svgContent = QTextStream(&file).readAll();
    file.close();

    QColor primary = Config::instance().primaryColor;
    if (dir == ChatDirection::Left) {
        svgContent.replace("__BG__", primary.name());
        svgContent.replace("__ICON__", "#FFFFFF");
    } else {
        svgContent.replace("__BG__", "#FFFFFF");
        svgContent.replace("__ICON__", primary.name());
    }
    QSvgRenderer renderer(svgContent.toUtf8());
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRect(0, 0, size, size));
    return pixmap;
}

/// 防止 URL 被拆行：在 / 和 : 后插入零宽 WORD JOINER（U+2060）。
/// Qt 断行遵循 UAX#14，/ 与 : 后本身就是合法断行点，http:// 会被拆到两行；
/// WJ 前后禁止断行，链接作为整体参与换行（整行放不下时仍按
/// WrapAtWordBoundaryOrAnywhere 兜底硬折，不会溢出气泡）。
QString preventUrlBreaking(const QString &text) {
    QString out;
    out.reserve(text.size() + 8);
    for (const QChar c: text) {
        out.append(c);
        if (c == QLatin1Char('/') || c == QLatin1Char(':')) {
            out.append(QChar(0x2060));
        }
    }
    return out;
}

/// 去除排版时插入的 WORD JOINER，避免复制文本被不可见字符污染。
QString stripJoiners(QString text) {
    text.remove(QChar(0x2060));
    text.replace(QChar(0x2029), QLatin1Char('\n'));  // selectedText 的段落分隔符
    return text;
}

/// 构造四角独立半径的矩形路径（MD3 非对称气泡：头像一侧带"尾巴"小角）。
void addCornersPath(QPainterPath &path, const QRectF &r, qreal tl, qreal tr, qreal br, qreal bl) {
    path.moveTo(r.left() + tl, r.top());
    path.lineTo(r.right() - tr, r.top());
    path.arcTo(QRectF(r.right() - 2 * tr, r.top(), 2 * tr, 2 * tr), 90, -90);
    path.lineTo(r.right(), r.bottom() - br);
    path.arcTo(QRectF(r.right() - 2 * br, r.bottom() - 2 * br, 2 * br, 2 * br), 0, -90);
    path.lineTo(r.left() + bl, r.bottom());
    path.arcTo(QRectF(r.left(), r.bottom() - 2 * bl, 2 * bl, 2 * bl), 270, -90);
    path.lineTo(r.left(), r.top() + tl);
    path.arcTo(QRectF(r.left(), r.top(), 2 * tl, 2 * tl), 180, -90);
    path.closeSubpath();
}
}  // namespace

// ============================================================================
// ChatTextBrowser
// ============================================================================

ChatTextBrowser::ChatTextBrowser(QWidget *parent) : QTextBrowser(parent) {
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    // 露出父级气泡背景
    setStyleSheet("background: transparent; border: none;");
    installEventFilter(this);
}

void ChatTextBrowser::setContentWidth(int width) {
    document()->setTextWidth(width);
}

int ChatTextBrowser::contentHeight() const {
    return qCeil(document()->size().height());
}

int ChatTextBrowser::idealContentWidth() const {
    return qCeil(document()->idealWidth());
}

void ChatTextBrowser::clearSelection() {
    QTextCursor cursor = textCursor();
    cursor.clearSelection();
    setTextCursor(cursor);
}

bool ChatTextBrowser::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            showContextMenu(mouseEvent->globalPosition().toPoint());
#else
            showContextMenu(mouseEvent->globalPos());
#endif
            return true;
        }
    } else if (event->type() == QEvent::ToolTip) {
        return true;
    }
    return QObject::eventFilter(obj, event);
}

void ChatTextBrowser::focusOutEvent(QFocusEvent *event) {
    if (!m_menuShowing) {
        clearSelection();
    }
    QTextBrowser::focusOutEvent(event);
}

void ChatTextBrowser::contextMenuEvent(QContextMenuEvent *event) {
    // 右键由 eventFilter 统一处理，抑制默认菜单
    event->ignore();
}

void ChatTextBrowser::showContextMenu(const QPoint &globalPos) {
    m_menuShowing = true;
    QMenu contextMenu(this);
    QAction *copyAction = contextMenu.addAction(tr("复制"));
    QAction *selectAllAction = contextMenu.addAction(tr("选择全部"));
    QAction *selected = contextMenu.exec(globalPos);
    if (selected == copyAction) {
        const QString plain = textCursor().hasSelection()
                                  ? textCursor().selectedText()
                                  : toPlainText();
        QApplication::clipboard()->setText(stripJoiners(plain));
    } else if (selected == selectAllAction) {
        selectAll();
    }
    m_menuShowing = false;
}

// ============================================================================
// BubbleFrame
// ============================================================================

BubbleFrame::BubbleFrame(ChatDirection dir, QWidget *parent)
        : QFrame(parent), m_dir(dir) {
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_content = new QVBoxLayout(this);
    m_content->setContentsMargins(0, 0, 0, 0);
    m_content->setSpacing(0);
}

void BubbleFrame::applyTheme() {
    update();  // 触发 paintEvent 重新取色
}

void BubbleFrame::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (!m_fillEnabled) {
        return;  // 图片等无边框图块：不绘制气泡底
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int radius = ChatTheme::bubbleRadius();
    const int tail = ChatTheme::bubbleTailRadius();
    qreal tl = radius, tr = radius, br = radius, bl = radius;
    if (m_dir == ChatDirection::Left) {
        bl = tail;  // 左下角朝向头像
    } else {
        br = tail;  // 右下角朝向头像
    }
    // 无描边纯填充：铺满整个 rect。不能内缩 0.5px（那是配合画笔描边的写法），
    // 否则高分屏下四周会留出一圈背景色环，看起来像气泡边缘色差。
    QRectF r = QRectF(rect());
    QPainterPath path;
    addCornersPath(path, r, tl, tr, br, bl);
    // MD3 去描边，纯色调分层
    painter.setPen(Qt::NoPen);
    painter.setBrush(ChatTheme::bubbleColor(m_dir));
    painter.drawPath(path);
}

// ============================================================================
// TextBubble
// ============================================================================

TextBubble::TextBubble(const QString &text, ChatDirection dir, QWidget *parent)
        : BubbleFrame(dir, parent) {
    m_browser = new ChatTextBrowser(this);
    m_browser->setObjectName(dir == ChatDirection::Left ? "chatTextLeft" : "chatTextRight");
    m_browser->setText(preventUrlBreaking(text));
    m_content->setContentsMargins(kPadding, 12, kPadding, 12);
    m_content->addWidget(m_browser);
}

void TextBubble::relayout(int availWidth) {
    if (m_browser == nullptr) {
        return;
    }
    int innerMax = availWidth - 2 * kPadding;  // 减去气泡左右留白
    if (innerMax < kMinWidth) {
        innerMax = kMinWidth;
    }
    // 先按最大可用宽度排版，得到理想内容宽度
    m_browser->setContentWidth(innerMax);
    int ideal = m_browser->idealContentWidth();
    int textWidth = qBound(kMinWidth, ideal, innerMax);
    m_browser->setContentWidth(textWidth);
    int h = m_browser->contentHeight();
    m_browser->setFixedWidth(textWidth);
    m_browser->setFixedHeight(qMax(16, h));
    m_browser->adjustSize();
}

// ============================================================================
// FileBubble
// ============================================================================

FileBubble::FileBubble(const QString &name, const QString &sizeStr, bool isDirectory,
                       ChatDirection dir, QWidget *parent)
        : BubbleFrame(dir, parent), m_isDirectory(isDirectory) {
    m_content->setContentsMargins(5, 5, 5, 5);

    // MD3 内衬卡片不再用子 QFrame + QSS border-radius（无抗锯齿、边缘色差），
    // 改由 FileBubble::paintEvent 直接抗锯齿自绘，布局留在卡片区域内
    auto *cardLayout = new QHBoxLayout;
    cardLayout->setSpacing(12);
    cardLayout->setContentsMargins(12, 10, 12, 10);

    m_icon = new QLabel(this);
    m_icon->setFixedSize(44, 44);

    auto *info = new QVBoxLayout;
    info->setSpacing(3);
    info->setContentsMargins(0, 0, 0, 0);

    m_name = new QLabel(name, this);
    m_name->setWordWrap(true);

    m_size = new QLabel(sizeStr, this);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(4);
    m_progress->setValue(0);

    m_status = new QLabel(this);
    QFont sf = m_status->font();
    sf.setPointSize(8);
    m_status->setFont(sf);
    m_status->setWordWrap(true);

    info->addWidget(m_name);
    info->addWidget(m_size);
    info->addWidget(m_progress);
    info->addWidget(m_status);

    cardLayout->addWidget(m_icon, 0, Qt::AlignVCenter);
    cardLayout->addLayout(info);
    m_content->addLayout(cardLayout);

    setMinimumWidth(240);
    setMaximumWidth(340);
    m_status->hide();
    applyTheme();
}

void FileBubble::setName(const QString &name) {
    m_name->setText(name);
}

void FileBubble::setSizeStr(const QString &sizeStr) {
    m_size->setText(sizeStr);
}

QPixmap FileBubble::renderFileIcon() const {
    if (m_isDirectory) {
        return recolorSvgPixmap(":/img/svgs/rc_dir_blue_icon.svg", 44, direction());
    }
    return recolorSvgPixmap(":/img/svgs/rc_file_icon_file.svg", 44, direction());
}

void FileBubble::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (!m_fillEnabled) {
        return;
    }
    // 不再分"气泡底色 + 内缩卡片叠加"两层：内缩边带会露出两层色差，
    // 看起来像气泡描了一圈边。这里预先将 MD3 叠加色合成到不透明底色，
    // 整个气泡单色绘制，边缘与内部完全一致。
    const QColor base = ChatTheme::bubbleColor(direction());
    const QColor overlay = direction() == ChatDirection::Right
                               ? QColor(255, 255, 255, 28)
                               : QColor(0, 0, 0, 16);
    const int a = overlay.alpha();
    const QColor merged((overlay.red() * a + base.red() * (255 - a)) / 255,
                        (overlay.green() * a + base.green() * (255 - a)) / 255,
                        (overlay.blue() * a + base.blue() * (255 - a)) / 255);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int radius = ChatTheme::bubbleRadius();
    const int tail = ChatTheme::bubbleTailRadius();
    qreal tl = radius, tr = radius, br = radius, bl = radius;
    if (direction() == ChatDirection::Left) {
        bl = tail;
    } else {
        br = tail;
    }
    QPainterPath path;
    addCornersPath(path, QRectF(rect()), tl, tr, br, bl);
    painter.setPen(Qt::NoPen);
    painter.setBrush(merged);
    painter.drawPath(path);
}

void FileBubble::applyTheme() {
    BubbleFrame::applyTheme();
    m_icon->setPixmap(renderFileIcon());
    m_name->setStyleSheet(QString("QLabel{ color: %1; background: transparent; font-weight: 600; }")
                                  .arg(ChatTheme::cssColor(ChatTheme::titleColor(direction()))));
    m_size->setStyleSheet(QString("QLabel{ color: %1; background: transparent; }")
                                  .arg(ChatTheme::cssColor(ChatTheme::subTextColor(direction()))));
    m_progress->setStyleSheet(QString(
            "QProgressBar{ background: %1; border: none; border-radius: 2px; }"
            "QProgressBar::chunk{ background: %2; border-radius: 2px; }")
                                      .arg(ChatTheme::cssColor(ChatTheme::progressTrackColor()),
                                           ChatTheme::cssColor(ChatTheme::progressChunkColor())));
    refreshStatusStyle();
}

void FileBubble::setProgress(int progress) {
    if (m_hasResult) {
        return;
    }
    m_progress->setValue(progress);
    m_progress->setVisible(progress > 0 && progress < 100);
}

void FileBubble::setResult(bool completed, const QString &filePath, bool received, bool showResult) {
    m_completed = completed;
    m_received = received;
    if (!filePath.isEmpty()) {
        m_filePath = filePath;
    }
    m_hasResult = showResult;
    m_progress->setVisible(!showResult);
    if (showResult) {
        m_status->setVisible(true);
    }
    refreshStatusStyle();
}

void FileBubble::refreshStatusStyle() {
    if (!m_hasResult) {
        m_status->setStyleSheet(QString("QLabel{ color: %1; background: transparent; }")
                                        .arg(ChatTheme::cssColor(ChatTheme::statusNeutralColor(direction()))));
        return;
    }
    QString text;
    QColor color;
    if (m_completed) {
        bool left = direction() == ChatDirection::Left;
        if (!left || FileUtils::exists(m_filePath)) {
            text = left ? tr("接收成功") : tr("发送成功");
            color = ChatTheme::successColor();
        } else {
            text = tr("文件已被删除");
            color = ChatTheme::errorColor();
        }
    } else {
        bool left = direction() == ChatDirection::Left;
        text = left ? tr("接收失败") : tr("发送失败");
        color = ChatTheme::errorColor();
    }
    m_status->setText(text);
    m_status->setStyleSheet(QString("QLabel{ color: %1; background: transparent; }").arg(ChatTheme::cssColor(color)));
}

// ============================================================================
// ImageBubble
// ============================================================================

ImageBubble::ImageBubble(const QString &path, const QString &name,
                         ChatDirection dir, QWidget *parent)
        : BubbleFrame(dir, parent), m_path(path), m_name(name) {
    setFillEnabled(false);  // MD3：图片不加气泡底，纯圆角图块
    m_content->setContentsMargins(0, 0, 0, 0);
    m_thumb = new QLabel(this);
    m_thumb->setFixedSize(180, 180);
    m_thumb->setAlignment(Qt::AlignCenter);
    m_thumb->setStyleSheet("QLabel{ background: rgba(0,0,0,40); color: #FFFFFF; border-radius: 12px; }");
    m_thumb->setText(tr("图片"));
    m_content->addWidget(m_thumb);
    applyTheme();
    // 进入事件循环后再懒加载缩略图，避免构造期阻塞
    QTimer::singleShot(0, this, &ImageBubble::loadThumbnail);
}

void ImageBubble::setPath(const QString &path) {
    m_path = path;
    m_loaded = false;
    loadThumbnail();
}

void ImageBubble::setResult(bool completed, const QString &filePath, bool received) {
    if (!filePath.isEmpty()) {
        m_path = filePath;
    }
    if (completed) {
        m_loaded = false;
        loadThumbnail();
        return;
    }
    // 失败：占位块显示失败文案（与 FileBubble 一致，按朝向区分接收/发送）
    Q_UNUSED(received)
    m_loaded = true;  // 阻止懒加载再次覆盖占位文案
    m_thumb->setStyleSheet("QLabel{ background: rgba(0,0,0,40); color: #FF7A6E; border-radius: 12px; }");
    m_thumb->setPixmap(QPixmap());
    m_thumb->setText(direction() == ChatDirection::Left ? tr("接收失败") : tr("发送失败"));
}

void ImageBubble::loadThumbnail() {
    if (m_loaded) {
        return;
    }
    if (m_path.isEmpty() || !FileUtils::exists(m_path)) {
        return;
    }
    QImage img = ThumbnailUtil::getThumnail(m_path, 200, 200);
    if (img.isNull()) {
        return;
    }
    m_loaded = true;
    QPixmap pm = QPixmap::fromImage(img).scaled(
        m_thumb->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    // 圆角裁剪
    QPixmap rounded(m_thumb->size());
    rounded.fill(Qt::transparent);
    QPainter p(&rounded);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addRoundedRect(QRect(QPoint(0, 0), m_thumb->size()), 12, 12);
    p.setClipPath(clip);
    p.drawPixmap(0, 0, pm);
    p.end();
    m_thumb->setStyleSheet("QLabel{ border: none; }");
    m_thumb->setText("");
    m_thumb->setPixmap(rounded);
}

void ImageBubble::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !m_path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_path));
        emit imageClicked(m_path);
    }
    BubbleFrame::mousePressEvent(event);
}

// ============================================================================
// ChatItemWidget
// ============================================================================

ChatItemWidget::ChatItemWidget(const ChatMessage &msg, QWidget *parent)
        : QWidget(parent), m_msg(msg) {
    build();
}

QPixmap ChatItemWidget::avatarPixmap(int devMode) {
    QPixmap src;
    if (devMode == Device::L_ANDROID) {
        src = QPixmap(":/img/ic_phone.png");
    } else if (devMode == Device::L_WIN) {
        src = QPixmap(":/img/ic_win.png");
    } else {
        src = QPixmap(":/img/ic_launcher_32.png");
    }
    // MD3 头像：主色淡底圆形容器 + 居中设备图标
    constexpr int kAvatarSize = 40;
    constexpr int kIconSize = 24;
    QPixmap out(kAvatarSize, kAvatarSize);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor bg = Config::instance().primaryColor;
    bg.setAlpha(50);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawEllipse(QRectF(0, 0, kAvatarSize, kAvatarSize));
    QPixmap icon = src.scaled(kIconSize, kIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawPixmap((kAvatarSize - icon.width()) / 2, (kAvatarSize - icon.height()) / 2, icon);
    painter.end();
    return out;
}

int ChatItemWidget::availableBubbleWidth() const {
    int base = 600;
    if (const QWidget *p = parentWidget()) {
        if (p->width() > 0) {
            base = p->width();
        }
    }
    int avail = static_cast<int>(base * 0.72) - 60;  // 预留头像与间距
    return qBound(200, avail, 500);
}

void ChatItemWidget::build() {
    const bool left = m_msg.direction == ChatDirection::Left;

    auto *main = new QHBoxLayout(this);
    main->setContentsMargins(12, 8, 12, 8);
    main->setSpacing(10);

    m_avatar = new QLabel(this);
    m_avatar->setFixedSize(40, 40);
    m_avatar->setPixmap(avatarPixmap(m_msg.devMode));

    m_column = new QVBoxLayout;
    m_column->setSpacing(4);
    m_column->setContentsMargins(0, 0, 0, 0);

    m_nickname = new QLabel(m_msg.deviceName, this);
    m_nickname->setAlignment(left ? Qt::AlignLeft : Qt::AlignRight);
    QFont nf = m_nickname->font();
    nf.setPointSize(8);
    m_nickname->setFont(nf);
    m_column->addWidget(m_nickname);

    for (const ContentElement &el: m_msg.elements) {
        BubbleFrame *bubble = nullptr;
        switch (el.type) {
            case ElementType::Text:
                bubble = new TextBubble(el.text, m_msg.direction, this);
                break;
            case ElementType::File: {
                auto *fb = new FileBubble(el.name, el.sizeStr, el.isDirectory, m_msg.direction, this);
                if (el.completed) {
                    fb->setResult(true, el.filePath, el.received, el.received || el.completed);
                }
                bubble = fb;
                break;
            }
            case ElementType::Image: {
                auto *ib = new ImageBubble(el.filePath, el.name, m_msg.direction, this);
                if (el.completed) {
                    ib->setResult(true, el.filePath, el.received);
                } else if (el.received) {
                    // 历史记录里未完成的图片视为已失败
                    ib->setResult(false, el.filePath, el.received);
                }
                bubble = ib;
                break;
            }
        }
        if (bubble == nullptr) {
            continue;
        }
        bubble->setElementUuid(el.elementUuid);
        m_column->addWidget(bubble, 0, left ? Qt::AlignLeft : Qt::AlignRight);
        m_bubbles.push_back(bubble);
        if (!el.elementUuid.isEmpty()) {
            m_bubbleByUuid.insert(el.elementUuid, bubble);
        }
    }

    if (left) {
        main->addWidget(m_avatar, 0, Qt::AlignTop);
        main->addLayout(m_column);
        main->addStretch();
    } else {
        main->addStretch();
        main->addLayout(m_column);
        main->addWidget(m_avatar, 0, Qt::AlignTop);
    }

    applyTheme();
    const int avail = availableBubbleWidth();
    for (BubbleFrame *b: m_bubbles) {
        b->relayout(avail);
    }
}

void ChatItemWidget::applyTheme() {
    m_nickname->setStyleSheet(QString("QLabel{ color: %1; background: transparent; }")
                                      .arg(ChatTheme::cssColor(ChatTheme::nicknameColor(m_msg.direction))));
    for (BubbleFrame *b: m_bubbles) {
        b->applyTheme();
    }
}

QSize ChatItemWidget::sizeHint() const {
    if (layout() != nullptr) {
        return layout()->sizeHint();
    }
    return QWidget::sizeHint();
}

void ChatItemWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::StyleChange) {
        applyTheme();
    }
    QWidget::changeEvent(event);
}

bool ChatItemWidget::setElementProgress(const QString &elementUuid, int progress) {
    ContentElement *el = m_msg.findElement(elementUuid);
    if (el == nullptr) {
        return false;
    }
    el->progress = progress;
    if (auto *fb = qobject_cast<FileBubble *>(m_bubbleByUuid.value(elementUuid))) {
        fb->setProgress(progress);
        return true;
    }
    return false;
}

bool ChatItemWidget::setElementResult(const QString &elementUuid, bool completed,
                                      const QString &filePath, bool received) {
    ContentElement *el = m_msg.findElement(elementUuid);
    if (el == nullptr) {
        return false;
    }
    el->completed = completed;
    el->received = received;
    el->progress = completed ? 100 : el->progress;
    if (!filePath.isEmpty()) {
        el->filePath = filePath;
    }
    BubbleFrame *bubble = m_bubbleByUuid.value(elementUuid);
    if (auto *fb = qobject_cast<FileBubble *>(bubble)) {
        fb->setResult(completed, filePath, received, true);
        return true;
    }
    if (auto *ib = qobject_cast<ImageBubble *>(bubble)) {
        ib->setResult(completed, filePath, received);
        return true;
    }
    return false;
}

void ChatItemWidget::resetAllProgress() {
    for (ContentElement &el: m_msg.elements) {
        if (el.type == ElementType::File && !el.completed) {
            el.progress = 0;
            if (auto *fb = qobject_cast<FileBubble *>(m_bubbleByUuid.value(el.elementUuid))) {
                fb->setProgress(0);
            }
        }
    }
}

bool ChatItemWidget::hasCompletedFile() const {
    const ContentElement *el = m_msg.firstOf(ElementType::File);
    if (el == nullptr) {
        el = m_msg.firstOf(ElementType::Image);
    }
    return el != nullptr && el->completed;
}

QString ChatItemWidget::firstFilePath() const {
    const ContentElement *file = m_msg.firstOf(ElementType::File);
    if (file != nullptr) {
        return file->filePath;
    }
    const ContentElement *img = m_msg.firstOf(ElementType::Image);
    return img ? img->filePath : QString();
}

QVector<LFile *> ChatItemWidget::collectFiles() const {
    return m_msg.collectFiles();
}

void ChatItemWidget::clearFilePointers() {
    for (ContentElement &el: m_msg.elements) {
        el.file = nullptr;
    }
}
