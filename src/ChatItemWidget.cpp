//
// ChatItemWidget 渲染层实现。
//
#include "ChatItemWidget.h"
#include "ChatTheme.h"
#include "Config.hpp"
#include "Device.h"
#include "LFile.h"
#include "FileUtils.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QShowEvent>
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
#include <QScreen>
#include <QGuiApplication>
#include <QCheckBox>
#include <QImageReader>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QSet>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QPointer>
#include <thread>

namespace {

/// 主题化 SVG 图标缓存：同一份 SVG 在每次 create 时都要“读资源 + 字符串替换 +
/// QSvgRenderer 解析 + 抗锯齿绘制”，一页文件消息重复十几次就是纯浪费。
/// key 含主色名，主题切换自然失效。
QHash<QString, QPixmap> g_svgPixmaps;
QHash<QString, QPixmap> g_avatarPixmaps;

/// 读取 SVG 内容并按主题替换颜色占位符，渲染成 pixmap。
/// 统一配色：图块底=白色、glyph=主题主色——左侧气泡改为浅灰蓝
/// surfaceVariant 后，白底蓝图标对比清晰；右侧主色气泡上同款，
/// 两侧不再区分（参考 Material You 文件卡片观感）
QPixmap recolorSvgPixmap(const QString &path, int size) {
    const QColor primary = Config::instance().primaryColor;
    const QString key = QString("%1|%2|%3").arg(path).arg(size).arg(primary.name());
    if (const auto hit = g_svgPixmaps.constFind(key); hit != g_svgPixmaps.constEnd()) {
        return hit.value();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QPixmap(size, size);
    }
    QString svgContent = QTextStream(&file).readAll();
    file.close();

    svgContent.replace("__BG__", "#FFFFFF");
    svgContent.replace("__ICON__", primary.name());
    QSvgRenderer renderer(svgContent.toUtf8());
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRect(0, 0, size, size));
    painter.end();
    if (g_svgPixmaps.size() > 64) {
        g_svgPixmaps.clear();
    }
    g_svgPixmaps.insert(key, pixmap);
    return pixmap;
}

/// 状态胶囊徽标：实心圆 + 白色对号/感叹号。32px 绘制、由 QLabel 缩到 16px
/// 显示（等效 2x 超采样，高分屏下不发虚）；颜色组合有限，静态缓存即可。
QPixmap statusBadgePixmap(const QColor &fill, bool success) {
    static QHash<QString, QPixmap> cache;
    const QString key = fill.name() + (success ? QLatin1String("|c") : QLatin1String("|e"));
    if (const auto hit = cache.constFind(key); hit != cache.constEnd()) {
        return hit.value();
    }
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawEllipse(QRectF(2, 2, 28, 28));
    if (success) {
        painter.setPen(QPen(Qt::white, 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(QPolygonF() << QPointF(9.6, 16.6) << QPointF(14.2, 21.2) << QPointF(22.6, 11.8));
    } else {
        painter.setPen(QPen(Qt::white, 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(16, 9.4), QPointF(16, 18.2));
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawEllipse(QPointF(16, 23.2), 1.9, 1.9);
    }
    painter.end();
    if (cache.size() > 32) {
        cache.clear();
    }
    cache.insert(key, pm);
    return pm;
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

// ---------------------------------------------------------------------------
// 后台缩略图解码：解码线程 + LRU 成品缓存
// ---------------------------------------------------------------------------

/// 成品位图缓存上限（条），按 LRU 淘汰，避免长时间翻看后内存无上限增长。
constexpr int kThumbCacheMax = 200;
/// 图片气泡圆角半径（逻辑像素）。
constexpr qreal kThumbCorner = 12.0;

/// 加载器是否已经创建：窗口析构阶段解除订阅时，不应凭空拉起解码线程。
bool g_thumbLoaderAlive = false;

struct ThumbJob {
    QString path;
    QSize devSize;   // 目标设备像素（含 DPR）
    qreal dpr = 1.0;
};

struct ThumbDecoded {
    QImage image;
    ThumbJob job;
};

/// 缓存键：路径 + 文件修改时间/大小 + 目标尺寸，图片被覆写后自动失效。
QString thumbCacheKey(const QString &path, const QSize &devSize) {
    const QFileInfo info(path);
    return QString("%1|%2|%3|%4x%5")
            .arg(QDir::toNativeSeparators(path))
            .arg(info.size())
            .arg(info.lastModified().toMSecsSinceEpoch())
            .arg(devSize.width())
            .arg(devSize.height());
}

/// 覆盖式解码：按铺满 devSize 所需的最小缩放读图。
/// QImageReader::setScaledSize 对 JPEG 等格式能在解码阶段直接降采样，
/// 比"整图解码 + SmoothTransformation 缩放"快一个数量级，也省掉几百 MB 的峰值内存。
QImage decodeCoverImage(const QString &path, const QSize &devSize) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize src = reader.size();
    if (!src.isValid() || src.isEmpty() || devSize.isEmpty()) {
        return QImage();
    }
    const qreal scale = qMax(qreal(devSize.width()) / src.width(),
                             qreal(devSize.height()) / src.height());
    if (scale < 1.0) {
        const QSize target(qCeil(src.width() * scale), qCeil(src.height() * scale));
        reader.setScaledSize(target);
    }
    QImage image = reader.read();
    if (image.isNull()) {
        return QImage();
    }
    if (image.width() < devSize.width() || image.height() < devSize.height()) {
        // 小图/解码器忽略缩放请求：交给 Qt 做一次平滑缩放兜底
        image = image.scaled(devSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

/// 居中裁剪 + 抗锯齿圆角，产出可直接交给 QLabel 的设备像素位图。
QPixmap buildRoundedThumb(const QImage &src, const QSize &devSize, qreal dpr) {
    if (src.isNull() || devSize.isEmpty()) {
        return QPixmap();
    }
    QPixmap out(devSize);
    out.setDevicePixelRatio(dpr);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QPainterPath clip;
    clip.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(devSize)), kThumbCorner * dpr, kThumbCorner * dpr);
    painter.setClipPath(clip);
    const QRect crop((src.width() - devSize.width()) / 2,
                     (src.height() - devSize.height()) / 2,
                     devSize.width(), devSize.height());
    painter.drawImage(QRect(QPoint(0, 0), devSize), src, crop);
    painter.end();
    return out;
}

class ThumbnailLoader {
public:
    static ThumbnailLoader &instance() {
        static ThumbnailLoader loader;
        g_thumbLoaderAlive = true;
        return loader;
    }

    /// 控件析构时解除订阅；加载器不存在（从未解过图）则直接返回。
    static void detach(ImageBubble *bubble) {
        if (!g_thumbLoaderAlive) {
            return;
        }
        instance().forget(bubble);
    }

    /// 命中成品缓存则返回，否则返回空位图（仅主线程调用）。
    QPixmap pixmap(const QString &key) {
        if (const auto it = m_pixmaps.constFind(key); it != m_pixmaps.constEnd()) {
            touch(key);
            return it.value();
        }
        return QPixmap();
    }

    void watch(const QString &key, ImageBubble *bubble) {
        QMutexLocker locker(&m_mutex);
        m_watchers[key].append(QPointer<ImageBubble>(bubble));
    }

    /// 解码请求：同一 key 只入队一次，重复滚动/翻页命中缓存后零开销。
    void request(const QString &key, const ThumbJob &job) {
        bool enqueue = false;
        {
            QMutexLocker locker(&m_mutex);
            if (!m_queued.contains(key)) {
                m_queued.insert(key);
                m_jobs.insert(key, job);
                m_queue.enqueue(key);
                enqueue = true;
            }
        }
        if (enqueue) {
            m_condition.wakeAll();
        }
    }

    ~ThumbnailLoader() {
        {
            QMutexLocker locker(&m_mutex);
            m_quit = true;
        }
        m_condition.wakeAll();
        // 先等解码线程退出，再让 m_sink 析构：避免向已销毁对象投递事件
        if (m_worker.joinable()) {
            m_worker.join();
        }
        g_thumbLoaderAlive = false;
    }

private:
    ThumbnailLoader() {
        m_worker = std::thread([this] { run(); });
    }

    ThumbnailLoader(const ThumbnailLoader &) = delete;
    ThumbnailLoader &operator=(const ThumbnailLoader &) = delete;

    void run() {
        forever {
            QString key;
            ThumbJob job;
            {
                QMutexLocker locker(&m_mutex);
                while (!m_quit && m_queue.isEmpty()) {
                    m_condition.wait(&m_mutex);
                }
                if (m_quit) {
                    return;
                }
                key = m_queue.dequeue();
                job = m_jobs.value(key);
            }
            ThumbDecoded decoded;
            decoded.job = job;
            decoded.image = decodeCoverImage(job.path, job.devSize);
            {
                QMutexLocker locker(&m_mutex);
                m_decoded.insert(key, decoded);
            }
            // QImage 可以跨线程，QPixmap 与控件回填必须回主线程
            QMetaObject::invokeMethod(&m_sink, [this, key] { deliver(key); }, Qt::QueuedConnection);
        }
    }

    /// 主线程：合成圆角位图、写缓存、回填仍然存活的订阅者。
    void deliver(const QString &key) {
        ThumbDecoded decoded;
        {
            QMutexLocker locker(&m_mutex);
            m_queued.remove(key);
            m_jobs.remove(key);
            if (!m_decoded.contains(key)) {
                return;
            }
            decoded = m_decoded.take(key);
        }
        const QPixmap pix = buildRoundedThumb(decoded.image, decoded.job.devSize, decoded.job.dpr);
        if (!pix.isNull()) {
            m_pixmaps.insert(key, pix);
            touch(key);
        }
        QList<QPointer<ImageBubble>> targets;
        {
            QMutexLocker locker(&m_mutex);
            targets = m_watchers.take(key);
        }
        for (const QPointer<ImageBubble> &target: targets) {
            if (!target.isNull()) {
                target->applyThumbnail(pix);
            }
        }
    }

    void forget(ImageBubble *bubble) {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_watchers.begin(); it != m_watchers.end(); ++it) {
            QList<QPointer<ImageBubble>> &list = it.value();
            for (int i = list.size() - 1; i >= 0; --i) {
                if (list.at(i) == bubble) {
                    list.removeAt(i);
                }
            }
        }
    }

    void touch(const QString &key) {
        m_order.removeAll(key);
        m_order.append(key);
        while (m_order.size() > kThumbCacheMax) {
            m_pixmaps.remove(m_order.takeFirst());
        }
    }

    QMutex m_mutex;
    QWaitCondition m_condition;
    QQueue<QString> m_queue;
    QSet<QString> m_queued;
    QHash<QString, ThumbJob> m_jobs;
    QHash<QString, ThumbDecoded> m_decoded;
    QHash<QString, QList<QPointer<ImageBubble>>> m_watchers;
    QHash<QString, QPixmap> m_pixmaps;   // 仅主线程读写
    QStringList m_order;                 // 仅主线程读写
    bool m_quit = false;
    std::thread m_worker;
    QObject m_sink;                      // 主线程事件接收器
};
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
        tl = tail;  // 左上角朝向头像（昵称在上方，尾巴靠头像一侧顶部）
    } else {
        tr = tail;  // 右上角朝向头像
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
    const int ideal = m_browser->idealContentWidth();
    if (ideal >= innerMax) {
        // 宽文本在最大宽度下就已撑满：文档布局无需重跑，直接复用当前高度，
        // 每条消息省掉两次全文重排（历史消息多时长文本是建页主要开销之一）
        m_browser->setFixedWidth(innerMax);
        m_browser->setFixedHeight(qMax(16, m_browser->contentHeight()));
        return;
    }
    const int textWidth = qBound(kMinWidth, ideal, innerMax);
    m_browser->setContentWidth(textWidth);
    m_browser->setFixedWidth(textWidth);
    m_browser->setFixedHeight(qMax(16, m_browser->contentHeight()));
}

// ============================================================================
// PillProgressBar
// ============================================================================

PillProgressBar::PillProgressBar(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void PillProgressBar::setValue(int value) {
    const int clamped = qBound(0, value, 100);
    if (m_value == clamped) {
        return;
    }
    m_value = clamped;
    update();
}

void PillProgressBar::setColors(const QColor &track, const QColor &chunk) {
    if (m_track == track && m_chunk == chunk) {
        return;
    }
    m_track = track;
    m_chunk = chunk;
    update();
}

void PillProgressBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const QRectF bar(QRectF(0, 0, width(), height()));
    const qreal radius = height() / 2.0;
    painter.setBrush(m_track);
    painter.drawRoundedRect(bar, radius, radius);
    if (m_value > 0 && width() > 0) {
        // 窄填充兜底拉伸到直径：保证两端都是完整半圆，不会退化成矩形；
        // 再裁剪到轨道圆角矩形，高进度时右端也不会溢出胶囊头
        qreal fillWidth = width() * m_value / 100.0;
        fillWidth = qMax(fillWidth, static_cast<qreal>(height()));
        QPainterPath clip;
        clip.addRoundedRect(bar, radius, radius);
        painter.setClipPath(clip);
        painter.setBrush(m_chunk);
        painter.drawRoundedRect(QRectF(0, 0, fillWidth, height()), radius, radius);
    }
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
    m_icon->setFixedSize(52, 52);

    auto *info = new QVBoxLayout;
    info->setSpacing(4);
    info->setContentsMargins(0, 0, 0, 0);

    m_name = new QLabel(name, this);
    m_name->setWordWrap(true);

    m_size = new QLabel(sizeStr, this);

    // 进度条：胶囊形、限宽左对齐；自绘避开 QProgressBar QSS chunk
    // 左端圆角拼接缺陷（低进度时渲染成矩形）
    m_progress = new PillProgressBar(this);
    m_progress->setFixedHeight(8);
    m_progress->setMaximumWidth(160);
    m_progress->setValue(0);
    auto *progressRow = new QHBoxLayout;
    progressRow->setContentsMargins(0, 0, 0, 0);
    progressRow->addWidget(m_progress);
    progressRow->addStretch();

    // 状态胶囊：圆形徽标 + 文字，圆角背景（MD3 chip 观感），宽度随内容
    m_statusChip = new QWidget(this);
    // 样式必须用 ID 选择器限定在容器自身：类型选择器 QWidget{...} 会级联到
    // 子 QLabel，带样式背景的 QLabel 绘制 pixmap 会丢失 alpha，透明区变黑块
    m_statusChip->setObjectName("fileBubbleStatusChip");
    m_statusChip->setAttribute(Qt::WA_StyledBackground, true);
    m_statusChip->setFixedHeight(24);
    m_statusChip->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    auto *chipLayout = new QHBoxLayout(m_statusChip);
    chipLayout->setContentsMargins(7, 0, 10, 0);
    chipLayout->setSpacing(5);
    m_statusIcon = new QLabel(m_statusChip);
    m_statusIcon->setFixedSize(16, 16);
    m_statusIcon->setScaledContents(true);
    m_statusText = new QLabel(m_statusChip);
    chipLayout->addWidget(m_statusIcon, 0, Qt::AlignVCenter);
    chipLayout->addWidget(m_statusText, 0, Qt::AlignVCenter);
    auto *chipRow = new QHBoxLayout;
    chipRow->setContentsMargins(0, 0, 0, 0);
    chipRow->addWidget(m_statusChip);
    chipRow->addStretch();

    info->addWidget(m_name);
    info->addWidget(m_size);
    info->addLayout(progressRow);
    info->addLayout(chipRow);

    cardLayout->addWidget(m_icon, 0, Qt::AlignVCenter);
    cardLayout->addLayout(info);
    m_content->addLayout(cardLayout);

    setMinimumWidth(240);
    setMaximumWidth(360);
    m_statusChip->hide();
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
        return recolorSvgPixmap(":/img/svgs/rc_dir_blue_icon.svg", 52);
    }
    return recolorSvgPixmap(":/img/svgs/rc_file_icon_file.svg", 52);
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
        tl = tail;  // 左上角朝向头像（与 BubbleFrame::paintEvent 一致）
    } else {
        tr = tail;  // 右上角朝向头像
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
    // 文件名加粗突出、大小次之（截图样式的层级对比）
    m_name->setStyleSheet(QString("QLabel{ color: %1; background: transparent; font-size: 13px; font-weight: 600; }")
                                  .arg(ChatTheme::cssColor(ChatTheme::titleColor(direction()))));
    m_size->setStyleSheet(QString("QLabel{ color: %1; background: transparent; font-size: 13px; }")
                                  .arg(ChatTheme::cssColor(ChatTheme::subTextColor(direction()))));
    m_progress->setColors(ChatTheme::progressTrackColor(direction()),
                          ChatTheme::progressChunkColor(direction()));
    m_statusChip->setStyleSheet(QString("QWidget#fileBubbleStatusChip{ background: %1; border-radius: 12px; }")
                                        .arg(ChatTheme::cssColor(ChatTheme::chipBgColor(direction()))));
    m_statusText->setStyleSheet(QString("QLabel{ color: %1; background: transparent; font-size: 13px; font-weight: 500; }")
                                        .arg(ChatTheme::cssColor(ChatTheme::titleColor(direction()))));
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
    // showResult=false 用于 restartElement 重置：同步隐藏状态胶囊，
    // 否则残留的“发送失败”不会消失
    m_statusChip->setVisible(showResult);
    refreshStatusStyle();
}

void FileBubble::refreshStatusStyle() {
    if (!m_hasResult) {
        return;  // 胶囊隐藏（传输中/已重置），无需刷新
    }
    QString text;
    QColor badge;
    bool ok = false;
    if (m_completed) {
        bool left = direction() == ChatDirection::Left;
        if (!left || FileUtils::exists(m_filePath)) {
            text = left ? tr("接收成功") : tr("发送成功");
            badge = ChatTheme::successColor();
            ok = true;
        } else {
            text = tr("文件已被删除");
            badge = ChatTheme::errorColor();
        }
    } else {
        // 左侧中断统一展示“未完成”（与参考图一致），右侧仍提示发送失败
        text = direction() == ChatDirection::Left ? tr("未完成") : tr("发送失败");
        badge = ChatTheme::errorColor();
    }
    m_statusText->setText(text);
    m_statusIcon->setPixmap(statusBadgePixmap(badge, ok));
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
    m_thumb->setFixedSize(160, 160);
    m_thumb->setAlignment(Qt::AlignCenter);
    m_thumb->setWordWrap(true);
    m_content->addWidget(m_thumb);
    applyTheme();
    // 传输完成前不加载图片，只显示占位（文件名 + 进度），完成后由 setResult 拉缩略图。
    updatePlaceholder();
}

ImageBubble::~ImageBubble() {
    // 控件可能在后台解码完成前就被删除（清空记录/退出）：先解除订阅再析构
    ThumbnailLoader::detach(this);
}

void ImageBubble::setThumbStyle(const QString &style) {
    if (m_thumbStyle == style) {
        return;
    }
    m_thumbStyle = style;
    m_thumb->setStyleSheet(style);
}

void ImageBubble::updatePlaceholder() {
    if (m_thumbApplied) {
        return;  // 缩略图已显示，占位不再覆盖
    }
    setThumbStyle(
        "QLabel{ background: rgba(0,0,0,90); color: #FFFFFF; border-radius: 12px; padding: 8px; }");
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (!m_thumb->pixmap().isNull()) {
        m_thumb->setPixmap(QPixmap());
    }
#else
    if (m_thumb->pixmap() != nullptr) {
        m_thumb->setPixmap(QPixmap());
    }
#endif
    const QString name = m_name.isEmpty() ? tr("图片") : m_name;
    if (m_completed) {
        // 已完成、缩略图还在后台队列里：只显示文件名，避免“100%”误导
        m_thumb->setText(name);
    } else {
        m_thumb->setText(QString("%1\n%2%").arg(name).arg(qBound(0, m_progress, 100)));
    }
}

void ImageBubble::setProgress(int progress) {
    if (m_completed) {
        return;
    }
    m_progress = progress;
    updatePlaceholder();
}

void ImageBubble::setPath(const QString &path) {
    m_path = path;
    m_loaded = false;
    m_thumbApplied = false;
    m_thumbRequested = false;
    requestThumbnail();
}

void ImageBubble::restart() {
    // 回到传输中状态：丢弃缩略图/失败文案，重新显示文件名 + 进度占位；
    // m_path 保留，重发期间仍可直接点击预览已有的本地文件
    m_completed = false;
    m_progress = 0;
    m_loaded = false;
    m_thumbApplied = false;
    m_thumbRequested = false;
    updatePlaceholder();
}

void ImageBubble::setResult(bool completed, const QString &filePath, bool received) {
    if (!filePath.isEmpty()) {
        m_path = filePath;
    }
    if (completed) {
        m_completed = true;
        m_progress = 100;
        m_loaded = false;
        m_thumbApplied = false;
        m_thumbRequested = false;
        requestThumbnail();
        return;
    }
    // 失败：占位块显示失败文案（与 FileBubble 一致，按朝向区分接收/发送）
    Q_UNUSED(received)
    m_loaded = true;  // 阻止懒加载再次覆盖占位文案
    setThumbStyle("QLabel{ background: rgba(0,0,0,40); color: #FF7A6E; border-radius: 12px; }");
    m_thumb->setPixmap(QPixmap());
    m_thumb->setText(direction() == ChatDirection::Left ? tr("接收失败") : tr("发送失败"));
}

void ImageBubble::requestThumbnail() {
    // 只有真正可见（滞留在视口内）才发请求：隐藏项不消耗解码，
    // 翻页时只解当前屏能看到的几张，其余滞动到时再按需拉取
    if (!m_completed || m_thumbApplied || m_thumbRequested || !isVisible()) {
        return;
    }
    if (m_path.isEmpty() || !FileUtils::exists(m_path)) {
        return;
    }
    const QSize box = m_thumb->size();  // 逻辑像素
    // 按屏幕设备像素比放大生成分辨率，避免高 DPI 下二次拉伸致模糊
    qreal dpr = 1.0;
    if (QScreen *scr = QGuiApplication::primaryScreen()) {
        dpr = scr->devicePixelRatio();
    }
    if (dpr < 1.0) {
        dpr = 1.0;
    }
    const QSize dev(qRound(box.width() * dpr), qRound(box.height() * dpr));
    m_thumbKey = thumbCacheKey(m_path, dev);
    if (const QPixmap cached = ThumbnailLoader::instance().pixmap(m_thumbKey); !cached.isNull()) {
        applyThumbnail(cached);
        return;
    }
    m_thumbRequested = true;
    updatePlaceholder();
    ThumbJob job;
    job.path = m_path;
    job.devSize = dev;
    job.dpr = dpr;
    ThumbnailLoader::instance().watch(m_thumbKey, this);
    ThumbnailLoader::instance().request(m_thumbKey, job);
}

void ImageBubble::applyThumbnail(const QPixmap &pixmap) {
    if (pixmap.isNull()) {
        return;  // 解码失败：保留占位（文件名），不影响双击大图预览
    }
    m_thumbApplied = true;
    m_loaded = true;
    setThumbStyle("QLabel{ border: none; }");
    m_thumb->setText("");
    m_thumb->setPixmap(pixmap);
}

void ImageBubble::showEvent(QShowEvent *event) {
    BubbleFrame::showEvent(event);
    requestThumbnail();
}

void ImageBubble::mousePressEvent(QMouseEvent *event) {
    // 不在这里直接拉外部查看器：交由主窗口的 ImagePreview 统一预览（可上下张浏览）
    if (event->button() == Qt::LeftButton && !m_path.isEmpty()) {
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
    const QColor bgColor = Config::instance().primaryColor;
    // 每条消息都要拼一个圆形头像：按（设备类型 + 主色）缓存，建页时只拼一次
    const QString key = QString("%1|%2").arg(devMode).arg(bgColor.name());
    if (const auto hit = g_avatarPixmaps.constFind(key); hit != g_avatarPixmaps.constEnd()) {
        return hit.value();
    }
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
    QColor bg = bgColor;
    bg.setAlpha(50);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawEllipse(QRectF(0, 0, kAvatarSize, kAvatarSize));
    QPixmap icon = src.scaled(kIconSize, kIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawPixmap((kAvatarSize - icon.width()) / 2, (kAvatarSize - icon.height()) / 2, icon);
    painter.end();
    if (g_avatarPixmaps.size() > 32) {
        g_avatarPixmaps.clear();
    }
    g_avatarPixmaps.insert(key, out);
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

    // 多选勾选框：默认隐藏，处于多选模式时由 setCheckMode 显示。
    m_check = new QCheckBox(this);
    m_check->setFixedSize(22, 22);
    m_check->setCursor(Qt::PointingHandCursor);
    m_check->setFocusPolicy(Qt::NoFocus);
    m_check->hide();
    connect(m_check, &QCheckBox::toggled, this, &ChatItemWidget::selectionChanged);

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
                } else if (el.progress > 0) {
                    // 传输中断的历史记录：恢复落库的近似进度（重启后不断点也能看到传到了哪）
                    fb->setProgress(el.progress);
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
                } else {
                    // 传输中：居中显示文件名 + 进度
                    ib->setProgress(el.progress);
                }
                connect(ib, &ImageBubble::imageClicked, this, &ChatItemWidget::imageRequested);
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
        main->addWidget(m_check, 0, Qt::AlignTop | Qt::AlignHCenter);
        main->addWidget(m_avatar, 0, Qt::AlignTop);
        main->addLayout(m_column);
        main->addStretch();
    } else {
        main->addStretch();
        main->addLayout(m_column);
        main->addWidget(m_avatar, 0, Qt::AlignTop);
        main->addWidget(m_check, 0, Qt::AlignTop | Qt::AlignHCenter);
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

void ChatItemWidget::setCheckMode(bool on) {
    m_checkMode = on;
    if (m_check == nullptr) {
        return;
    }
    if (!on) {
        m_check->setChecked(false);
    }
    m_check->setVisible(on);
}

bool ChatItemWidget::isSelected() const {
    return m_check != nullptr && m_check->isChecked();
}

void ChatItemWidget::setSelected(bool sel) {
    if (m_check != nullptr) {
        m_check->setChecked(sel);
    }
}

void ChatItemWidget::mousePressEvent(QMouseEvent *event) {
    // 多选模式下，点击行背景（未被子气泡消费的区）切换勾选
    if (m_checkMode && event->button() == Qt::LeftButton) {
        if (m_check != nullptr) {
            m_check->setChecked(!m_check->isChecked());
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

bool ChatItemWidget::setElementProgress(const QString &elementUuid, int progress) {
    ContentElement *el = m_msg.findElement(elementUuid);
    if (el == nullptr) {
        return false;
    }
    if (el->progress == progress) {
        return true;  // 同一百分比的重复回调不再重设子控件、不再拉重绘
    }
    el->progress = progress;
    BubbleFrame *bubble = m_bubbleByUuid.value(elementUuid);
    if (auto *fb = qobject_cast<FileBubble *>(bubble)) {
        fb->setProgress(progress);
        return true;
    }
    if (auto *ib = qobject_cast<ImageBubble *>(bubble)) {
        ib->setProgress(progress);
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

bool ChatItemWidget::restartElement(const QString &elementUuid, LFile *file, mlong size, const QString &sizeStr) {
    ContentElement *el = m_msg.findElement(elementUuid);
    if (el == nullptr) {
        return false;
    }
    el->file = file;
    el->size = size;
    if (!sizeStr.isEmpty()) {
        el->sizeStr = sizeStr;
    }
    el->progress = 0;
    el->completed = false;
    el->received = false;
    BubbleFrame *bubble = m_bubbleByUuid.value(elementUuid);
    if (auto *fb = qobject_cast<FileBubble *>(bubble)) {
        fb->setSizeStr(el->sizeStr);
        // showResult=false：清除失败/成功文案并恢复进度条轨道
        fb->setResult(false, QString(), false, false);
        fb->setProgress(0);
        return true;
    }
    if (auto *ib = qobject_cast<ImageBubble *>(bubble)) {
        ib->restart();
        return true;
    }
    return false;
}

void ChatItemWidget::resetAllProgress() {
    for (ContentElement &el: m_msg.elements) {
        if (!el.completed && (el.type == ElementType::File || el.type == ElementType::Image)) {
            el.progress = 0;
            BubbleFrame *bubble = m_bubbleByUuid.value(el.elementUuid);
            if (auto *fb = qobject_cast<FileBubble *>(bubble)) {
                fb->setProgress(0);
            } else if (auto *ib = qobject_cast<ImageBubble *>(bubble)) {
                ib->setProgress(0);
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
