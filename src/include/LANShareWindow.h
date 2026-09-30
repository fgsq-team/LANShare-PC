#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include "Message.h"
#include "Device.h"
#include "MessageFile.h"
#include "MessageDB_V3.h"
#include "LFile.h"
#include "EditTextEventFilter.h"
#include <QListWidgetItem>
#include <QSystemTrayIcon>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QScrollBar>
#include <QStyledItemDelegate>

namespace Ui {
    class LANShareWindow;
}
class LANShare;

class AcceptFiles;

class SmoothScrollHandler : public QObject {
Q_OBJECT
public:
    explicit SmoothScrollHandler(QListWidget *listWidget) : QObject(listWidget), m_listWidget(listWidget) {
        // 安装事件过滤器以监听鼠标滚轮事件
        m_listWidget->viewport()->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (obj == m_listWidget->viewport()) {
            if (event->type() == QEvent::Wheel) {
                auto *wheelEvent = dynamic_cast<QWheelEvent *>(event);
                // 计算滚动的距离，并确保滚动方向正确
                int numSteps = -(wheelEvent->angleDelta().y() / 120); // 使用负数来翻转滚动方向
                // 启动定时器开始平滑滚动
                if (numSteps != 0) {
                    int scrollAmount = m_pixelsPerStep * (numSteps > 0 ? 1 : -1);
                    m_listWidget->verticalScrollBar()->setValue(
                            m_listWidget->verticalScrollBar()->value() + scrollAmount);
                }
                return true; // 拦截鼠标滚轮事件
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QListWidget *m_listWidget;
    const int m_pixelsPerStep = 30; // 减小每步滚动的像素数
};


class LANShareWindow : public QMainWindow {
Q_OBJECT


public:
    explicit LANShareWindow(QWidget *parent = nullptr);

    void dealMessage(Message *messageW, QListWidgetItem *item) const;
    void insertMessage(Message *messageW, QListWidgetItem *item) const;

    // Set window to top layer
    void setWindowToTopLayer();

    bool getClipboard();

    static LANShareWindow *getInstance();

    void updateWebServiceIp() const;

    ~LANShareWindow() override;

signals:

    void sigNewMessage(Device device, QString message, bool isLeft = false);

    void sigNewWebClient(QString token, QString ip, QString name);

    void sigRecviceFile(LFile *lFile, QString uuid, QString message, QString userName, mlong fileSize,
                        bool left, bool isFile, bool completed);

    void sigRecviceFileProgress(QString uuid, int progress);

    void sigRecviceFileSuccess(QString uuid, bool completed, QString filePath);

    void sigRequstRecvFiles(AcceptFiles *acceptFiles);

    void sigUpdateProgress();

    void sigCopyText(QString text);


public slots:

    void on_sendButton_clicked();

    void on_select_send_devices_clicked();

    void on_select_files_clicked();

    void on_select_directorys_clicked();

    void newMessage(Device device, QString message, bool isLeft);

    void newWebClient(const QString &token, const QString &ip, const QString &name);

    void recviceFile(LFile *lFile, const QString &uuid, QString message, QString userName, mlong fileSize, bool left,
                     bool isFile, bool completed);

    void recviceFileProgress(const QString &uuid, int progress);

    void recviceFileSuccess(const QString &uuid, bool completed, const QString &filePath);

    void requstRecvFiles(AcceptFiles *acceptFiles);

    bool eventFilter(QObject *watched, QEvent *event);

    void dragEnterEvent(QDragEnterEvent *event);

    void dropEvent(QDropEvent *e);

    void on_setting_triggered();

void deleteTime(const QString &uuid);

void on_actionclaerAll_triggered();

    void on_actionclearMessage_triggered();

    void on_actionclearFile_triggered();

    void on_actionabout_triggered();

    void on_chatListWidget_itemDoubleClicked(QListWidgetItem *item);

    void on_chatListWidget_itemClicked(QListWidgetItem *item);

    void on_chatListWidget_customContextMenuRequested(const QPoint &pos);

    void updateProgress();

    void updateSetting();

    void copyText(const QString &text);

    void on_webService_clicked();

    void checkVersionCallback(QNetworkReply *reply);

void checkAndAddChatTime(const QString &bindId);

void loadDeviceList();

    void onScroll(int value);

public:
    void sendMessage(bool isClip = false);

    void showDeviceSelecter(std::vector<LFile *> &fileaPaths);

    void createTrayIcon();

    void closeTrayIcon();

    void checkVersion();

    void loadData();

private:
    void closeEvent(QCloseEvent *event) override;

    // 统一处理拖入的文件/文件夹 URL（主窗口 dropEvent 与子控件事件过滤器共用）
    void handleDroppedUrls(const QList<QUrl> &urls);

protected:
    void resizeEvent(QResizeEvent *event);

    void keyPressEvent(QKeyEvent *event);

private:
    Ui::LANShareWindow *ui;
    std::vector<Device> devices;
    Device currentDevice;
    QSystemTrayIcon *trayIcon = nullptr;
    QNetworkAccessManager *networkAccessManager = nullptr;
    SmoothScrollHandler *smoothScrollHandler = nullptr;
    int pageSize = 10;
    int pageCount = 0;
    bool loadingData = false;
    mlong lastMessageTime = 0;
};

#endif // MAINWINDOW_H
