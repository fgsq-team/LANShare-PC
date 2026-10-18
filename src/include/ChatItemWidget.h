//
// 聊天条目控件：ChatMessage 数据的渲染产物。
//
// 结构：头像 + （昵称 + 若干内容气泡）纵向列，朝向镜像。
// 每个内容元素渲染成一个 BubbleFrame 子类（文本/文件/图片），
// 气泡背景由 BubbleFrame::paintEvent 依据自身 rect() 绘制——不再像旧实现
// 那样引用兄弟控件/布局坐标手绘，从根本上避免了错位与性能问题。
//
#ifndef LANSHARE_CHATITEMWIDGET_H
#define LANSHARE_CHATITEMWIDGET_H

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QTextEdit>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QClipboard>
#include <QProgressBar>
#include <QMap>
#include "ChatMessage.h"

class LFile;
class QCheckBox;

/**
 * 聊天正文文本浏览器。
 * 文字颜色 / 选区颜色由 QSS 的 #chatTextLeft / #chatTextRight 控制，
 * 背景保持透明以露出 BubbleFrame 绘制的气泡。
 */
class ChatTextBrowser : public QTextBrowser {
Q_OBJECT
public:
    explicit ChatTextBrowser(QWidget *parent = nullptr);

    /// 供外部依据文档内容计算气泡尺寸。
    void setContentWidth(int width);
    int contentHeight() const;
    int idealContentWidth() const;
    void clearSelection();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void showContextMenu(const QPoint &globalPos);
    bool m_menuShowing = false;
};

/**
 * 气泡基类：只负责按自身几何绘制圆角背景/描边，并托管内容布局。
 */
class BubbleFrame : public QFrame {
Q_OBJECT
public:
    explicit BubbleFrame(ChatDirection dir, QWidget *parent = nullptr);

    ChatDirection direction() const { return m_dir; }
    const QString &elementUuid() const { return m_elementUuid; }
    void setElementUuid(const QString &uuid) { m_elementUuid = uuid; }

    virtual void applyTheme();
    virtual void relayout(int availWidth) { (void) availWidth; }

    /** 是否绘制气泡底色（图片气泡关闭，呈现纯圆角图块）。 */
    void setFillEnabled(bool enabled) { m_fillEnabled = enabled; }

protected:
    void paintEvent(QPaintEvent *event) override;

    ChatDirection m_dir;
    QString m_elementUuid;
    QVBoxLayout *m_content = nullptr;
    bool m_fillEnabled = true;
};

/**
 * 文本气泡。
 */
class TextBubble : public BubbleFrame {
Q_OBJECT
public:
    explicit TextBubble(const QString &text, ChatDirection dir, QWidget *parent = nullptr);
    void relayout(int availWidth) override;

private:
    ChatTextBrowser *m_browser = nullptr;
    static constexpr int kPadding = 20;      // 气泡内上下左右留白
    static constexpr int kMinWidth = 40;
};

/**
 * 胶囊形进度条（自绘）。
 * QProgressBar 的 QSS chunk 在贴靠进度条起始端时圆角拼接有缺陷：
 * 低进度时填充块左端渲染成矩形，宽度超过半径后才变圆角。这里直接
 * 手绘轨道与填充两端全圆角的胶囊，窄填充兜底拉伸到直径保证两端半圆。
 */
class PillProgressBar : public QWidget {
Q_OBJECT
public:
    explicit PillProgressBar(QWidget *parent = nullptr);

    void setValue(int value);
    int value() const { return m_value; }
    void setColors(const QColor &track, const QColor &chunk);

    QSize sizeHint() const override { return QSize(160, 8); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_value = 0;
    QColor m_track;
    QColor m_chunk;
};

/**
 * 文件/文件夹气泡：图标 + 名称 + 大小 + 进度条 + 状态胶囊（圆形徽标 + 文字）。
 */
class FileBubble : public BubbleFrame {
Q_OBJECT
public:
    FileBubble(const QString &name, const QString &sizeStr, bool isDirectory,
               ChatDirection dir, QWidget *parent = nullptr);

    void setProgress(int progress);
    void setResult(bool completed, const QString &filePath, bool received, bool showResult);
    void applyTheme() override;

    void setName(const QString &name);
    void setSizeStr(const QString &sizeStr);

protected:
    /// 自绘 MD3 内衬卡片（QSS border-radius 无抗锯齿，边缘会有色差）。
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap renderFileIcon() const;
    void refreshStatusStyle();

    QLabel *m_icon = nullptr;
    QLabel *m_name = nullptr;
    QLabel *m_size = nullptr;
    QWidget *m_statusChip = nullptr;   // 状态胶囊容器（圆角背景）
    QLabel *m_statusIcon = nullptr;    // 胶囊内的圆形徽标
    QLabel *m_statusText = nullptr;    // 胶囊内的状态文字
    PillProgressBar *m_progress = nullptr;
    bool m_isDirectory = false;
    bool m_completed = false;
    bool m_received = false;
    bool m_hasResult = false;
    QString m_filePath;
};

/**
 * 图片气泡：传输中居中显示文件名 + 进度，成功后才加载缩略图 + 点击预览。
 *
 * 缩略图解码全部交给 ThumbnailLoader（后台线程 + LRU 缓存）：
 * 历史消息里一张原图动辄几千万像素，如果在构造时同步整图解码，
 * 分页加载会一次性卡住主线程几百毫秒，这里改为"控件可见才请求、
 * 解完回填"，翻页与滚动都不会被解码阻塞。
 */
class ImageBubble : public BubbleFrame {
Q_OBJECT
public:
    ImageBubble(const QString &path, const QString &name,
                ChatDirection dir, QWidget *parent = nullptr);

    ~ImageBubble() override;

    void setPath(const QString &path);

    /// 传输进度：未完成时在图片中心显示文件名 + 百分比。
    void setProgress(int progress);

    /// 传输结果回写：成功刷新缩略图，失败显示占位文案。
    void setResult(bool completed, const QString &filePath, bool received);

    /// 重新发送时回到传输中状态：丢弃缩略图/失败文案，恢复文件名 + 进度占位。
    void restart();

    /// 后台解码完成后由 ThumbnailLoader 在主线程回调（pixmap 为空表示解码失败）。
    void applyThumbnail(const QPixmap &pixmap);

signals:
    void imageClicked(const QString &path);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    /// 请求缩略图：命中缓存立即回填，否则排队等后台解码（仅在控件可见时发起）。
    void requestThumbnail();
    /// 未完成占位：深色圆角块上居中显示文件名与进度百分比。
    void updatePlaceholder();
    /// 仅在样式真的变化时重设样式表（setStyleSheet 会强制 polish + 重绘）。
    void setThumbStyle(const QString &style);

    QLabel *m_thumb = nullptr;
    QString m_path;
    QString m_name;
    QString m_thumbKey;
    QString m_thumbStyle;   // 已应用到 m_thumb 的样式表，避免重复 setStyleSheet 触发重绘
    bool m_loaded = false;
    bool m_completed = false;
    bool m_thumbApplied = false;
    bool m_thumbRequested = false;
    int m_progress = 0;
};

/**
 * 聊天条目组装器。对应 QListWidget 里一条 item 的 widget。
 */
class ChatItemWidget : public QWidget {
Q_OBJECT
public:
    explicit ChatItemWidget(const ChatMessage &msg, QWidget *parent = nullptr);

    const ChatMessage &message() const { return m_msg; }
    ChatMessage &message() { return m_msg; }
    QString uuid() const { return m_msg.uuid; }

    bool isFileMessage() const { return m_msg.hasMedia(); }
    bool isTextMessage() const { return !m_msg.hasMedia(); }

    // --- 多选模式 ---
    /// 进入/退出多选模式：切换勾选框可见性，退出时清空勾选。
    void setCheckMode(bool on);
    bool isCheckMode() const { return m_checkMode; }
    bool isSelected() const;
    void setSelected(bool sel);

    // --- 元素级状态更新（按 elementUuid 定位）---
    bool setElementProgress(const QString &elementUuid, int progress);
    bool setElementResult(const QString &elementUuid, bool completed,
                          const QString &filePath, bool received);
    void resetAllProgress();

    /// 重发同一文件时复用原消息：按 elementUuid 把元素重置为传输中状态，
    /// 重新绑定运行期 LFile 并刷新大小；命中返回 true。
    bool restartElement(const QString &elementUuid, LFile *file, mlong size, const QString &sizeStr);

    // --- 交互数据访问 ---
    QString displayText() const { return m_msg.displayText(); }
    bool hasCompletedFile() const;
    QString firstFilePath() const;
    QVector<LFile *> collectFiles() const;
    void clearFilePointers();

    void applyTheme();

    QSize sizeHint() const override;

signals:
    /// 图片气泡被点击，请求在预览窗口打开该图（由主窗口汇聚整段会话的图片列表统一处理）。
    void imageRequested(const QString &path);

    /// 多选模式下勾选状态变化，主窗口据此刷新“已选 N 项”计数与全选态。
    void selectionChanged();

protected:
    void changeEvent(QEvent *event) override;
    /// 多选模式下列在空白区域的点击也切换勾选（子气泡自行消费的事件不在此列）。
    void mousePressEvent(QMouseEvent *event) override;

private:
    void build();
    int availableBubbleWidth() const;
    static QPixmap avatarPixmap(int devMode);

    ChatMessage m_msg;
    QLabel *m_avatar = nullptr;
    QLabel *m_nickname = nullptr;
    QVBoxLayout *m_column = nullptr;
    QVector<BubbleFrame *> m_bubbles;
    QMap<QString, BubbleFrame *> m_bubbleByUuid;
    QCheckBox *m_check = nullptr;   // 多选勾选框（默认隐藏）
    bool m_checkMode = false;       // 是否处于多选模式
};

#endif //LANSHARE_CHATITEMWIDGET_H
