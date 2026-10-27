#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include "ChatMessage.h"
#include "ChatItemWidget.h"
#include "Device.h"
#include "MessageDB.h"
#include "LFile.h"
#include "EditTextEventFilter.h"
#include <QListWidgetItem>
#include <QSystemTrayIcon>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QWheelEvent>

namespace Ui {
    class LANShareWindow;
}
class LANShare;

class AcceptFiles;
class QCheckBox;
class QLabel;

/// 聊天列表平滑滚动：拦截滚轮事件，逐帧插值把滚动条推向目标位置。
///
/// 旧实现每收到一个滚轮事件就重启一段 150ms OutCubic 动画：事件间隔小于时长时
/// 缓动曲线不断从头重建，速度突变；而且单格 240px 位移配整数 startValue/endValue，
/// 高分屏下既“飞”又“顿”，看起来就是不跟手。这里改成 60fps 定时器驱动的指数逼近：
/// 目标位置用小数码累加（不截断），每帧只移动剩余量的一部分，惯性连贯且跟随及时。
class SmoothScrollHandler : public QObject {
Q_OBJECT
public:
    explicit SmoothScrollHandler(QListWidget *listWidget) : QObject(listWidget), m_listWidget(listWidget) {
        m_listWidget->viewport()->installEventFilter(this);
        m_timer.setInterval(kFrameMs);
        m_timer.setTimerType(Qt::PreciseTimer);
        connect(&m_timer, &QTimer::timeout, this, &SmoothScrollHandler::tick);
    }

    /// 终止惯性并同步目标：分页顶部插入前必须调用，
    /// 否则下一帧会把旧目标位置盖回来，画面“一弹一弹”。
    void cancel() {
        m_timer.stop();
        m_target = m_listWidget->verticalScrollBar()->value();
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj != m_listWidget->viewport() || event->type() != QEvent::Wheel) {
            return QObject::eventFilter(obj, event);
        }
        auto *wheel = static_cast<QWheelEvent *>(event);
        QScrollBar *bar = m_listWidget->verticalScrollBar();
        // 触控板优先用 pixelDelta；angleDelta 换算保留小数——旧实现整除 /120
        // 会把高精度设备的小增量截断为 0，滚轮被吞但不动（“阻力”根因）
        int delta = wheel->pixelDelta().y();
        if (delta != 0) {
            m_gain = kTouchGain;
        } else {
            if (wheel->angleDelta() == QPoint(0, 0)) {
                return false;  // 空增量（横向滚动等）交回默认处理
            }
            delta = static_cast<int>(wheel->angleDelta().y() * kPixelsPerNotch / 120.0);
            m_gain = kWheelGain;
        }
        if (delta == 0) {
            return false;
        }
        // 惯性进行中在旧目标上累加（连续滚动不中断），否则从当前位置起步
        const qreal base = m_timer.isActive() ? m_target : bar->value();
        m_target = qBound<qreal>(bar->minimum(), base - delta, bar->maximum());
        if (!m_timer.isActive()) {
            m_timer.start();
        }
        return true;
    }

private:
    /// 每帧向目标逼近剩余量的一部分，直到亚像素误差内归位。
    void tick() {
        QScrollBar *bar = m_listWidget->verticalScrollBar();
        m_target = qBound<qreal>(bar->minimum(), m_target, bar->maximum());
        const qreal current = bar->value();
        const qreal diff = m_target - current;
        if (qAbs(diff) < 1.0) {
            const int goal = qRound(m_target);
            if (bar->value() != goal) {
                bar->setValue(goal);
            }
            m_timer.stop();
            return;
        }
        qreal step = diff * m_gain;
        if (qAbs(step) < 1.0) {
            step = diff > 0 ? 1.0 : -1.0;  // 保证每帧有位移，不留长尾
        }
        bar->setValue(qRound(current + step));
    }

    QListWidget *m_listWidget;
    QTimer m_timer;
    qreal m_target = 0;
    qreal m_gain = kWheelGain;
    static constexpr int kFrameMs = 16;         // ~60fps
    static constexpr int kPixelsPerNotch = 160;  // 标准一格（120 单位）滚动像素
    static constexpr qreal kWheelGain = 0.30;   // 滚轮惯性：约 3 帧收敛
    static constexpr qreal kTouchGain = 0.55;   // 触控板本身就是逐像素事件，紧跟手
};


class LANShareWindow : public QMainWindow {
Q_OBJECT


public:
    explicit LANShareWindow(QWidget *parent = nullptr);

    void handleMessage(ChatItemWidget *messageW, QListWidgetItem *item);
    void insertMessage(ChatItemWidget *messageW, QListWidgetItem *item);

    // Set window to top layer
    void setWindowToTopLayer();

    bool getClipboard();

    static LANShareWindow *getInstance();

    void updateWebServiceIp();

    ~LANShareWindow() override;

signals:

    void sigNewMessage(Device device, QString message, bool isLeft = false);

    void sigNewWebClient(QString token, QString ip);

    void sigReceiveFile(LFile *lFile, QString uuid, QString message, QString userName, mlong fileSize,
                        bool left, bool isFile, bool completed);

    void sigReceiveFileProgress(QString uuid, int progress);

    void sigReceiveFileSuccess(QString uuid, bool completed, QString filePath);

    void sigRequestRecvFiles(AcceptFiles *acceptFiles);

    void sigUpdateProgress();

    void sigCopyText(QString text);


public slots:

    void on_sendButton_clicked();

    void on_selectSendDevices_clicked();

    void on_selectFiles_clicked();

    void on_selectDirectories_clicked();

    void newMessage(Device device, QString message, bool isLeft);

    void newWebClient(QString token, QString ip);

    void receiveFile(LFile *lFile, const QString &uuid, QString message, QString userName, mlong fileSize, bool left,
                     bool isFile, bool completed);

    void receiveFileProgress(const QString &uuid, int progress);

    void receiveFileSuccess(const QString &uuid, bool completed, const QString &filePath);

    void requestRecvFiles(AcceptFiles *acceptFiles);

    bool eventFilter(QObject *watched, QEvent *event);

    void dragEnterEvent(QDragEnterEvent *event);

    void dropEvent(QDropEvent *e);

    void on_setting_triggered();

    void on_actionClearAll_triggered();

    void on_actionClearMessage_triggered();

    void on_actionClearFile_triggered();

    void on_actionAbout_triggered();

    // 打开“扫码连接”二维码对话框，供手机扫描后与本机互加设备
    void on_actionScanConnect_triggered();

    // 打开“网页二维码”对话框，供手机扫码访问本机网页版地址
    void on_actionWebQrCode_triggered();

    void on_chatListWidget_itemDoubleClicked(QListWidgetItem *item);

    void on_chatListWidget_itemClicked(QListWidgetItem *item);

    void on_chatListWidget_customContextMenuRequested(const QPoint &pos);

    void updateProgress();

    void updateSetting();

    void copyText(const QString &text);

    void on_webService_clicked();

    void loadDeviceList();

    void onScroll(int value);

    /// 打开应用内图片预览：汇聚会话中已完成的图片作为浏览列表，定位到被点击的那张。
    void openImagePreview(const QString &path);

    // --- 消息多选删除 ---
    /// 进入多选模式（可预先选中某条）。
    void enterSelectMode(QListWidgetItem *preselect = nullptr);
    /// 退出多选模式，清空所有勾选并隐藏操作栏。
    void exitSelectMode();
    /// 全选/取消全选（由顶部“全选”复选框驱动）。
    void onSelectAllToggled(bool checked);
    /// 任意条目勾选变化时刷新计数与全选态。
    void refreshSelectionState();
    /// 删除所有已勾选消息（同步从 DB 与列表移除）。
    void deleteSelectedMessages();

public:
    void sendMessage(bool isClip = false);

    void showDeviceSelecter(std::vector<LFile *> &filePaths);

    /// 重新发送消息中的文件/图片。
    /// V5 断点续传：以相同源路径 + 大小 + 本机 UUID 生成稳定 fileId，
    /// 接收端若仍持有对应 .part 分段则从断点继续，已完成的分段已落地
    /// 故会整体重新发送；对旧版本设备退化为全量重发。
    void resendMessageFiles(ChatItemWidget *message);

    void createTrayIcon();

    void closeTrayIcon();

    void loadData();

private:
    void closeEvent(QCloseEvent *event) override;

protected:
    void resizeEvent(QResizeEvent *event);

    void keyPressEvent(QKeyEvent *event);

private:
    Ui::LANShareWindow *ui;
    std::vector<Device> devices;
    Device currentDevice;
    QSystemTrayIcon *trayIcon = nullptr;
    SmoothScrollHandler *smoothScrollHandler = nullptr;
    int pageSize = 10;
    int pageCount = 0;
    bool loadingData = false;
    /// 向上分页已穷尽（最近一次查询返回空）：避免滚到顶部后每次微抖动都重跑 COUNT + 分页查询
    bool noMoreData = false;
    /// 已排入下一轮事件循环的建页请求（防重复投递）
    bool pendingLoad = false;

    // --- 多选模式状态 ---
    bool m_selectMode = false;
    QWidget *m_selectBar = nullptr;        // 顶部多选操作栏（平时隐藏）
    QCheckBox *m_selectAllCheck = nullptr; // 全选
    QLabel *m_selectCount = nullptr;       // 已选 N 项

    /// 构建顶部多选操作栏并插入聊天列表上方（默认隐藏）。
    void buildSelectBar();
    /// 取 item 对应的 ChatItemWidget（可能为 nullptr）。
    ChatItemWidget *widgetOfItem(QListWidgetItem *item) const;
};

#endif // MAINWINDOW_H
