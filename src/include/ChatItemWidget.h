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
 * 文件/文件夹气泡：图标 + 名称 + 大小 + 进度条 + 状态。
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
    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    bool m_isDirectory = false;
    bool m_completed = false;
    bool m_received = false;
    bool m_hasResult = false;
    QString m_filePath;
};

/**
 * 图片气泡：缩略图 + 点击预览（懒加载，进入布局时异步读取缩略图）。
 */
class ImageBubble : public BubbleFrame {
Q_OBJECT
public:
    ImageBubble(const QString &path, const QString &name,
                ChatDirection dir, QWidget *parent = nullptr);

    void setPath(const QString &path);

    /// 传输结果回写：成功刷新缩略图，失败显示占位文案。
    void setResult(bool completed, const QString &filePath, bool received);

signals:
    void imageClicked(const QString &path);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void loadThumbnail();
    QLabel *m_thumb = nullptr;
    QString m_path;
    QString m_name;
    bool m_loaded = false;
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

    // --- 元素级状态更新（按 elementUuid 定位）---
    bool setElementProgress(const QString &elementUuid, int progress);
    bool setElementResult(const QString &elementUuid, bool completed,
                          const QString &filePath, bool received);
    void resetAllProgress();

    // --- 交互数据访问 ---
    QString displayText() const { return m_msg.displayText(); }
    bool hasCompletedFile() const;
    QString firstFilePath() const;
    QVector<LFile *> collectFiles() const;
    void clearFilePointers();

    void applyTheme();

    QSize sizeHint() const override;

protected:
    void changeEvent(QEvent *event) override;

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
};

#endif //LANSHARE_CHATITEMWIDGET_H
