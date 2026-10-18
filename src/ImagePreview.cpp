#include "ImagePreview.h"

#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QTransform>
#include <QWindow>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QTimer>
#include <cmath>

namespace {
/// 统一的滚轮焦点坐标（Qt6 用 position()，Qt5 用 posF()）。
QPointF wheelPos(QWheelEvent *e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position();
#else
    return QPointF(e->posF());
#endif
}

/// 统一的鼠标事件坐标。
QPoint mousePos(QMouseEvent *e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

constexpr double kMinScale = 0.02;
constexpr double kMaxScale = 40.0;
}  // namespace

ImagePreview::ImagePreview(const QStringList &filePaths, int index, QWidget *parent)
        : QWidget(parent), m_files(filePaths) {
    // 带 parent 也不会内嵌到主窗口：显式声明为独立顶层窗口，仅借 parent 归并生命周期/任务栏归属
    setWindowFlag(Qt::Window, true);
    setWindowTitle(tr("图片预览"));
    setMinimumSize(360, 260);
    resize(960, 680);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    buildToolBar();
    buildSideNav();

    if (index < 0 || index >= m_files.size()) {
        index = m_files.isEmpty() ? -1 : 0;
    }
    loadImage(index);
}

void ImagePreview::buildToolBar() {
    m_toolBar = new QWidget(this);
    m_toolBar->setObjectName("imagePreviewToolBar");
    m_toolBar->setStyleSheet(
            "#imagePreviewToolBar{ background: rgba(28,28,30,215); border-radius: 10px; }"
            "QPushButton{ background: transparent; color: #F2F2F2; border: none;"
            " padding: 5px 9px; border-radius: 6px; font-size: 15px; }"
            "QPushButton:hover{ background: rgba(255,255,255,40); }"
            "QPushButton:disabled{ color: rgba(255,255,255,70); }");

    auto *bar = new QHBoxLayout(m_toolBar);
    bar->setContentsMargins(8, 4, 8, 4);
    bar->setSpacing(2);

    QStyle *st = style();
    auto makeBtn = [this](const QIcon &icon, const QString &text, const QString &tip) {
        auto *b = new QPushButton(text, m_toolBar);
        if (!icon.isNull()) {
            b->setIcon(icon);
            b->setText(QString());
        }
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };

    m_prevBtn = makeBtn(st->standardIcon(QStyle::SP_ArrowLeft), QString(), tr("上一张 (←)"));
    connect(m_prevBtn, &QPushButton::clicked, this, &ImagePreview::goPrev);
    bar->addWidget(m_prevBtn);

    m_nextBtn = makeBtn(st->standardIcon(QStyle::SP_ArrowRight), QString(), tr("下一张 (→)"));
    connect(m_nextBtn, &QPushButton::clicked, this, &ImagePreview::goNext);
    bar->addWidget(m_nextBtn);

    bar->addSpacing(6);

    QPushButton *zoomOut = makeBtn(QIcon(), QStringLiteral("−"), tr("缩小 (-)"));
    connect(zoomOut, &QPushButton::clicked, this, &ImagePreview::zoomOut);
    bar->addWidget(zoomOut);

    m_zoomLabel = new QLabel("100%", m_toolBar);
    m_zoomLabel->setMinimumWidth(120);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setStyleSheet("color:#DDDDDD; font-size:13px;");
    bar->addWidget(m_zoomLabel);

    QPushButton *zoomIn = makeBtn(QIcon(), QStringLiteral("+"), tr("放大 (+)"));
    connect(zoomIn, &QPushButton::clicked, this, &ImagePreview::zoomIn);
    bar->addWidget(zoomIn);

    bar->addSpacing(6);

    QPushButton *fit = makeBtn(QIcon(), tr("适应"), tr("适应窗口 (F)"));
    connect(fit, &QPushButton::clicked, this, &ImagePreview::fitWindow);
    bar->addWidget(fit);

    QPushButton *actual = makeBtn(QIcon(), tr("原始"), tr("原始尺寸 (1)"));
    connect(actual, &QPushButton::clicked, this, &ImagePreview::actualSize);
    bar->addWidget(actual);

    bar->addSpacing(6);

    QPushButton *rotL = makeBtn(QIcon(), QStringLiteral("⟲"), tr("向左旋转 (R)"));
    connect(rotL, &QPushButton::clicked, this, &ImagePreview::rotateCcw);
    bar->addWidget(rotL);

    QPushButton *rotR = makeBtn(QIcon(), QStringLiteral("⟳"), tr("向右旋转 (Shift+R)"));
    connect(rotR, &QPushButton::clicked, this, &ImagePreview::rotateCw);
    bar->addWidget(rotR);

    bar->addSpacing(6);

    QPushButton *copy = makeBtn(QIcon(), QStringLiteral("复制"), tr("复制到剪贴板 (C)"));
    connect(copy, &QPushButton::clicked, this, &ImagePreview::copyToClipboard);
    bar->addWidget(copy);

    QPushButton *save = makeBtn(st->standardIcon(QStyle::SP_DialogSaveButton), QString(), tr("另存为 (S)"));
    connect(save, &QPushButton::clicked, this, &ImagePreview::saveAs);
    bar->addWidget(save);

    QPushButton *open = makeBtn(st->standardIcon(QStyle::SP_DirOpenIcon), QString(), tr("用外部程序打开 (O)"));
    connect(open, &QPushButton::clicked, this, &ImagePreview::openExternal);
    bar->addWidget(open);

    QPushButton *full = makeBtn(st->standardIcon(QStyle::SP_TitleBarMaxButton), QString(), tr("全屏 (F / Esc)"));
    connect(full, &QPushButton::clicked, this, &ImagePreview::toggleFullScreen);
    bar->addWidget(full);

    m_toolBar->adjustSize();
    m_toolBar->raise();

    // 顶部短暂提示标签（默认隐藏）
    m_toast = new QLabel(this);
    m_toast->setObjectName("imagePreviewToast");
    m_toast->setStyleSheet(
            "#imagePreviewToast{ background: rgba(28,28,30,220); color: #F2F2F2;"
            " border-radius: 8px; padding: 6px 14px; font-size: 13px; }");
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->hide();
}

void ImagePreview::buildSideNav() {
    // 左右两侧的悬浮箭头：覆盖在画面边缘，点击切换上/下一张，仅在多图时可见。
    const QString arrowQss =
            "QPushButton{ background: rgba(28,28,30,150); color: #F2F2F2; border: none;"
            " border-radius: 22px; font-size: 26px; }"
            "QPushButton:hover{ background: rgba(60,60,64,220); }"
            "QPushButton:disabled{ color: rgba(255,255,255,60); }";

    m_sidePrev = new QPushButton(QStringLiteral("\u2039"), this);
    m_sidePrev->setObjectName("imagePreviewSidePrev");
    m_sidePrev->setFixedSize(44, 44);
    m_sidePrev->setCursor(Qt::PointingHandCursor);
    m_sidePrev->setFocusPolicy(Qt::NoFocus);
    m_sidePrev->setToolTip(tr("上一张 (\u2190)"));
    m_sidePrev->setStyleSheet(arrowQss);
    connect(m_sidePrev, &QPushButton::clicked, this, &ImagePreview::goPrev);

    m_sideNext = new QPushButton(QStringLiteral("\u203a"), this);
    m_sideNext->setObjectName("imagePreviewSideNext");
    m_sideNext->setFixedSize(44, 44);
    m_sideNext->setCursor(Qt::PointingHandCursor);
    m_sideNext->setFocusPolicy(Qt::NoFocus);
    m_sideNext->setToolTip(tr("下一张 (\u2192)"));
    m_sideNext->setStyleSheet(arrowQss);
    connect(m_sideNext, &QPushButton::clicked, this, &ImagePreview::goNext);

    m_sidePrev->raise();
    m_sideNext->raise();
}

void ImagePreview::loadImage(int index) {
    m_index = index;
    m_rotation = 0;
    m_fitMode = true;
    m_offset = QPointF(0, 0);
    m_scale = 1.0;
    if (index >= 0 && index < m_files.size()) {
        m_pixmap = QPixmap(m_files.at(index));
        m_loaded = !m_pixmap.isNull();
        const QFileInfo fi(m_files.at(index));
        setWindowTitle(QString("%2  [%1/%3]").arg(index + 1)
                                               .arg(fi.fileName())
                                               .arg(m_files.size()));
    } else {
        m_pixmap = QPixmap();
        m_loaded = false;
    }
    if (m_prevBtn) m_prevBtn->setEnabled(m_files.size() > 1 && m_index > 0);
    if (m_nextBtn) m_nextBtn->setEnabled(m_files.size() > 1 && m_index >= 0 && m_index < m_files.size() - 1);
    // 左右悬浮箭头：仅多图时显示；到达首/尾端时禁用对应方向。
    const bool multiImage = m_files.size() > 1;
    if (m_sidePrev) {
        m_sidePrev->setVisible(multiImage);
        m_sidePrev->setEnabled(m_index > 0);
    }
    if (m_sideNext) {
        m_sideNext->setVisible(multiImage);
        m_sideNext->setEnabled(m_index >= 0 && m_index < m_files.size() - 1);
    }
    updateZoomLabel();
    update();
}

double ImagePreview::fitScale() const {
    if (m_pixmap.isNull()) {
        return 1.0;
    }
    QSize disp = m_pixmap.size();
    if (m_rotation % 180 != 0) {
        disp.transpose();
    }
    const QSize view = size();
    if (disp.isEmpty() || view.width() <= 0 || view.height() <= 0) {
        return 1.0;
    }
    return qMin(view.width() / static_cast<double>(disp.width()),
               view.height() / static_cast<double>(disp.height()));
}

double ImagePreview::effectiveScale() const {
    return m_fitMode ? fitScale() : m_scale;
}

void ImagePreview::applyScale(double newScale, const QPointF &anchor) {
    if (!m_loaded) {
        return;
    }
    const double cur = effectiveScale();
    newScale = qBound(kMinScale, newScale, kMaxScale);
    if (qFuzzyCompare(cur, newScale)) {
        return;
    }
    if (m_fitMode) {
        // 从适应模式切到手动，先把当前适应比作为基准，避免跳变
        m_scale = cur;
        m_fitMode = false;
    }
    // 以 anchor（窗口坐标）为不动点缩放：pivot 为 anchor 相对窗口中心的向量
    const QPointF pivot = anchor - QPointF(rect().center());
    m_offset = pivot - (pivot - m_offset) * (newScale / cur);
    m_scale = newScale;
    updateZoomLabel();
    update();
}

void ImagePreview::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter p(this);
    p.fillRect(rect(), QColor(24, 24, 26));

    if (!m_loaded) {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, tr("无法加载图片"));
        return;
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const double s = effectiveScale();
    p.translate(rect().center() + m_offset);
    if (m_rotation != 0) {
        p.rotate(m_rotation);
    }
    p.scale(s, s);
    const QSize ps = m_pixmap.size();
    p.drawPixmap(QRectF(-ps.width() / 2.0, -ps.height() / 2.0, ps.width(), ps.height()),
                 m_pixmap, QRectF(0, 0, ps.width(), ps.height()));
}

void ImagePreview::wheelEvent(QWheelEvent *event) {
    if (!m_loaded) {
        return;
    }
    const double steps = event->angleDelta().y() / 120.0;
    const double factor = std::pow(1.25, steps);
    applyScale(effectiveScale() * factor, wheelPos(event));
}

void ImagePreview::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStart = mousePos(event);
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImagePreview::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
        const QPoint now = mousePos(event);
        m_offset += QPointF(now - m_dragStart);
        m_dragStart = now;
        update();
    }
}

void ImagePreview::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
    }
}

void ImagePreview::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_fitMode) {
            actualSize();
        } else {
            fitWindow();
        }
    }
}

void ImagePreview::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            return;
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            zoomOut();
            return;
        case Qt::Key_0:
            fitWindow();
            return;
        case Qt::Key_1:
            actualSize();
            return;
        case Qt::Key_R:
            if (event->modifiers() & Qt::ShiftModifier) {
                rotateCcw();
            } else {
                rotateCw();
            }
            return;
        case Qt::Key_Left:
        case Qt::Key_A:
            goPrev();
            return;
        case Qt::Key_Right:
        case Qt::Key_D:
            goNext();
            return;
        case Qt::Key_F:
            toggleFullScreen();
            return;
        case Qt::Key_C:
            copyToClipboard();
            return;
        case Qt::Key_S:
            saveAs();
            return;
        case Qt::Key_O:
            openExternal();
            return;
        case Qt::Key_Escape:
            if (m_fullScreen) {
                toggleFullScreen();
            } else {
                close();
            }
            return;
        default:
            break;
    }
    QWidget::keyPressEvent(event);
}

void ImagePreview::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event)
    if (m_toolBar) {
        m_toolBar->adjustSize();
        const int x = (width() - m_toolBar->width()) / 2;
        const int y = height() - m_toolBar->height() - 14;
        m_toolBar->move(x, qMax(6, y));
        m_toolBar->raise();
    }
    // 左右悬浮箭头：竖直居中，贴左右边缘留 12px 间距。
    if (m_sidePrev) {
        m_sidePrev->move(12, (height() - m_sidePrev->height()) / 2);
        m_sidePrev->raise();
    }
    if (m_sideNext) {
        m_sideNext->move(width() - m_sideNext->width() - 12, (height() - m_sideNext->height()) / 2);
        m_sideNext->raise();
    }
    // 适应模式下随窗口尺寸变化重算比例
    if (m_fitMode) {
        updateZoomLabel();
        update();
    }
}

void ImagePreview::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (m_centeredOnce || m_fullScreen) {
        return;
    }
    m_centeredOnce = true;
    // 首次打开：根据所在屏幕可用区域限制大小并居中，让图片一开始就居中呈现
    QScreen *scr = screen();
    if (scr == nullptr) {
        scr = QGuiApplication::primaryScreen();
    }
    if (scr == nullptr) {
        return;
    }
    const QRect avail = scr->availableGeometry();
    const int maxW = static_cast<int>(avail.width() * 0.8);
    const int maxH = static_cast<int>(avail.height() * 0.8);
    if (width() > maxW || height() > maxH) {
        resize(qMin(width(), maxW), qMin(height(), maxH));
    }
    move(avail.center() - rect().center());
    // move/resize 会触发 resizeEvent 重算 fit；这里再保证一次居中刷新
    update();
}

void ImagePreview::zoomIn() {
    applyScale(effectiveScale() * 1.25, QPointF(rect().center()));
}

void ImagePreview::zoomOut() {
    applyScale(effectiveScale() / 1.25, QPointF(rect().center()));
}

void ImagePreview::fitWindow() {
    m_fitMode = true;
    m_offset = QPointF(0, 0);
    updateZoomLabel();
    update();
}

void ImagePreview::actualSize() {
    m_fitMode = false;
    m_scale = 1.0;
    m_offset = QPointF(0, 0);
    updateZoomLabel();
    update();
}

void ImagePreview::rotateCw() {
    if (!m_loaded) {
        return;
    }
    m_rotation = (m_rotation + 90) % 360;
    updateZoomLabel();
    update();
}

void ImagePreview::rotateCcw() {
    if (!m_loaded) {
        return;
    }
    m_rotation = (m_rotation + 270) % 360;
    updateZoomLabel();
    update();
}

void ImagePreview::goPrev() {
    if (m_index > 0) {
        loadImage(m_index - 1);
    }
}

void ImagePreview::goNext() {
    if (m_index >= 0 && m_index < m_files.size() - 1) {
        loadImage(m_index + 1);
    }
}

void ImagePreview::toggleFullScreen() {
    if (m_fullScreen) {
        showNormal();
        m_fullScreen = false;
    } else {
        showFullScreen();
        m_fullScreen = true;
    }
    if (windowHandle()) {
        windowHandle()->requestActivate();
    }
}

QPixmap ImagePreview::rotatedPixmap() const {
    if (!m_loaded || m_rotation == 0) {
        return m_pixmap;
    }
    QTransform t;
    t.rotate(m_rotation);
    return m_pixmap.transformed(t, Qt::SmoothTransformation);
}

void ImagePreview::copyToClipboard() {
    if (m_loaded) {
        QApplication::clipboard()->setPixmap(rotatedPixmap());
        statusBarFlash(tr("已复制到剪贴板"));
    }
}

void ImagePreview::saveAs() {
    if (!m_loaded) {
        return;
    }
    const QFileInfo fi(m_files.value(m_index));
    QString suggested = fi.fileName();
    const QString path = QFileDialog::getSaveFileName(this, tr("另存为"), suggested,
                                                      tr("图片 (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;所有文件 (*.*)"));
    if (!path.isEmpty()) {
        if (!rotatedPixmap().save(path)) {
            statusBarFlash(tr("保存失败"));
        }
    }
}

void ImagePreview::openExternal() {
    if (m_index >= 0 && m_index < m_files.size()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_files.at(m_index)));
    }
}

void ImagePreview::statusBarFlash(const QString &text) {
    if (!m_toast) {
        return;
    }
    m_toast->setText(text);
    m_toast->adjustSize();
    m_toast->move((width() - m_toast->width()) / 2, 16);
    m_toast->show();
    m_toast->raise();
    QTimer::singleShot(1600, m_toast, &QLabel::hide);
}

void ImagePreview::updateZoomLabel() {
    if (!m_zoomLabel) {
        return;
    }
    if (!m_loaded) {
        m_zoomLabel->setText("0%");
        return;
    }
    const int percent = static_cast<int>(std::lround(effectiveScale() * 100.0));
    QSize disp = m_pixmap.size();
    if (m_rotation % 180 != 0) {
        disp.transpose();
    }
    const QString mode = m_fitMode ? tr("适应") : tr("手动");
    m_zoomLabel->setText(QString("%1%  %2×%3  ·  %4")
                                 .arg(percent)
                                 .arg(disp.width())
                                 .arg(disp.height())
                                 .arg(mode));
}
