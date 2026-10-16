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
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QWheelEvent>

namespace Ui {
    class LANShareWindow;
}
class LANShare;

class AcceptFiles;

/// 聊天列表平滑滚动：拦截滚轮事件，用缓动动画把滚动条推进到目标位置。
class SmoothScrollHandler : public QObject {
Q_OBJECT
public:
    explicit SmoothScrollHandler(QListWidget *listWidget) : QObject(listWidget), m_listWidget(listWidget) {
        m_listWidget->viewport()->installEventFilter(this);
        QScrollBar *bar = m_listWidget->verticalScrollBar();
        m_animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_animation, &QVariantAnimation::valueChanged, this,
                [bar](const QVariant &value) { bar->setValue(value.toInt()); });
        // 非动画滚动（拖滚动条/键盘/scrollTo）后同步目标位置，避免下次滚轮回跳
        connect(bar, &QScrollBar::valueChanged, this, [this](int value) {
            if (m_animation.state() != QAbstractAnimation::Running) {
                m_target = value;
            }
        });
    }

    /// 终止进行中的缓动动画：分页插入恢复滚动位置前必须调用，
    /// 否则动画 tick 会用旧 endValue 覆盖恢复后的位置，画面"一弹一弹"。
    void cancel() { m_animation.stop(); }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj != m_listWidget->viewport() || event->type() != QEvent::Wheel) {
            return QObject::eventFilter(obj, event);
        }
        auto *wheel = static_cast<QWheelEvent *>(event);
        QScrollBar *bar = m_listWidget->verticalScrollBar();
        // 触控板优先用 pixelDelta；angleDelta 换算保留小数——旧实现整除 /120
        // 会把高精度设备的小增量截断为 0，滚轮被吞但不动（"阻力"根因）
        int delta = wheel->pixelDelta().y();
        if (delta == 0) {
            delta = static_cast<int>(wheel->angleDelta().y() * kPixelsPerNotch / 120.0);
        }
        if (delta == 0) {
            return true;
        }
        const int from = bar->value();
        // 动画进行中在旧目标上累加，连续滚动不中断叠加
        const int base = m_animation.state() == QAbstractAnimation::Running ? m_target : from;
        m_target = qBound(bar->minimum(), base - delta, bar->maximum());
        m_animation.stop();
        m_animation.setDuration(kAnimationMs);
        m_animation.setStartValue(from);
        m_animation.setEndValue(m_target);
        m_animation.start();
        return true;
    }

private:
    QListWidget *m_listWidget;
    QVariantAnimation m_animation;
    int m_target = 0;
    static constexpr int kPixelsPerNotch = 240;  // 标准一格（120 单位）滚动像素 ≈ 7 行
    static constexpr int kAnimationMs = 150;     // 缓动时长
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

    void on_chatListWidget_itemDoubleClicked(QListWidgetItem *item);

    void on_chatListWidget_itemClicked(QListWidgetItem *item);

    void on_chatListWidget_customContextMenuRequested(const QPoint &pos);

    void updateProgress();

    void updateSetting();

    void copyText(const QString &text);

    void on_webService_clicked();

    void loadDeviceList();

    void onScroll(int value);

public:
    void sendMessage(bool isClip = false);

    void showDeviceSelecter(std::vector<LFile *> &filePaths);

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
};

#endif // MAINWINDOW_H
