//
// 图片预览窗口：脱离系统查看器，在应用内直接预览聊天的图片消息。
//
// 能力：滚轮/按钮/键盘缩放、按住拖拽平移、左右键点切换适应/原始尺寸、
// 90° 旋转、上一张/下一张在整段会话图片间浏览、另存为、复制到剪贴板、
// 外部程序打开、全屏。图像由 paintEvent 依据 scale/offset/rotation 手绘，
// 工具栏是覆盖在底部的子控件，不参与布局，避免与图像绘制互相挤压。
//
#ifndef LANSHARE_IMAGEPREVIEW_H
#define LANSHARE_IMAGEPREVIEW_H

#include <QWidget>
#include <QPixmap>
#include <QStringList>
#include <QPointF>

class QLabel;
class QPushButton;

/**
 * 图片预览窗口。
 */
class ImagePreview : public QWidget {
Q_OBJECT
public:
    /// filePaths 为可浏览的图片路径列表（用于上/下张切换），index 为初始展示下标。
    explicit ImagePreview(const QStringList &filePaths, int index = 0, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void zoomIn();
    void zoomOut();
    void fitWindow();
    void actualSize();
    void rotateCw();
    void rotateCcw();
    void goPrev();
    void goNext();
    void toggleFullScreen();
    void saveAs();
    void copyToClipboard();
    void openExternal();

private:
    void buildToolBar();
    /// 左右两侧的悬浮上一张/下一张箭头（仅在多图时可见）。
    void buildSideNav();
    void loadImage(int index);
    void applyScale(double newScale, const QPointF &anchor);
    void updateZoomLabel();
    /// 在窗口顶部短暂提示一条信息（1.6s 后自动消失）。
    void statusBarFlash(const QString &text);
    QPixmap rotatedPixmap() const;

    double fitScale() const;      // 适应窗口时的缩放比
    double effectiveScale() const; // 当前生效缩放比（自动/手动）

    QStringList m_files;
    int m_index = -1;
    QPixmap m_pixmap;
    bool m_loaded = false;

    double m_scale = 1.0;          // 手动缩放比（非适应模式生效）
    QPointF m_offset;              // 图像中心相对窗口中心的像素偏移
    int m_rotation = 0;            // 0/90/180/270
    bool m_fitMode = true;         // 是否适应窗口

    bool m_dragging = false;
    QPoint m_dragStart;
    bool m_fullScreen = false;
    bool m_centeredOnce = false;   // 首次显示时把窗口居中于屏幕

    QLabel *m_zoomLabel = nullptr;
    QLabel *m_toast = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_sidePrev = nullptr;
    QPushButton *m_sideNext = nullptr;
    QWidget *m_toolBar = nullptr;
};

#endif //LANSHARE_IMAGEPREVIEW_H
