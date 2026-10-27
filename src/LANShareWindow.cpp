#include <QTextBlock>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QDateTime>
#include <QDesktopServices>
#include <QIcon>
#include <QUuid>
#include <QScrollBar>
#include <QHash>

#include <qmimedata.h>
#include <QClipboard>
#include <vector>
#include <QProcess>
#include <utility>
#include <QBuffer>
#include <QMessageBox>
#include <QSystemTrayIcon>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include "LANShareWindow.h"
#include "ui_LANShareWindow.h"
#include "Setting.h"
#include "Utils.h"
#include "DeviceSelecter.h"
#include "About.h"
#include "CopyableTextDialog.h"
#include "EditTextEventFilter.h"
#include "LANShare.h"
#include "FileUtils.h"
#include "UpdateChecker.h"
#include "ImagePreview.h"
#include "QRConnectDialog.h"
#include "WebUrlQRDialog.h"

LANShareWindow *lanShareWindow = nullptr;

namespace {

/// 进度落库节流：回调按数据块频率触发，每次都 UPDATE 整行扫不住 SQLite，
/// 变化满 5% （以及 0/100 两端）才写一次；崩溃/断连后重启可恢复到近似进度
QHash<QString, int> g_lastSavedProgress;

void persistProgressThrottled(ChatItemWidget *message, const QString &uuid, int progress) {
    if (progress >= 100) {
        MessageDB::instance().updateMessage(message->message());
        g_lastSavedProgress.remove(uuid);
        return;
    }
    int &last = g_lastSavedProgress[uuid];
    if (progress <= 0 || qAbs(progress - last) >= 5) {
        last = progress;
        MessageDB::instance().updateMessage(message->message());
    }
}

} // namespace

void LANShareWindow::showDeviceSelecter(std::vector<LFile *> &filePaths) {
    auto *deviceSelecter = new DeviceSelecter([filePaths](const Device &device) {
        std::thread tSendfile([device, filePaths]() {
            LANShare::getInstance()->legacyFileTransfer.sendFile(device, filePaths, filePaths.size());
        });
        tSendfile.detach();
    }, false, LANShare::getInstance(), this);
    deviceSelecter->show();
}


LANShareWindow::LANShareWindow(QWidget *parent) : QMainWindow(parent),
                                                  ui(new Ui::LANShareWindow) {
    lanShareWindow = this;
    UpdateChecker::check(this, false);
    ui->setupUi(this);
    // 逐像素滚动 + 关闭“按内容调尺寸”：后者会在每次插入 item 时重算整个
    // 内容尺寸，是分页与滚动掉帧的来头之一（在 .ui 里也已改为 AdjustIgnored）
    ui->chatListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->chatListWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->chatListWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    ui->chatListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    // ui->chatListWidget->installEventFilter(this);
    connect(ui->chatListWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, &LANShareWindow::onScroll);
    ui->textEdit->installEventFilter(this);
    // 使聊天列表滚动平滑
    smoothScrollHandler = new SmoothScrollHandler(ui->chatListWidget);
    ui->textEdit->installEventFilter(EditTextEventFilter::getInstance());
    ui->textEdit->setAcceptDrops(false);
    // 禁止富文本
    ui->textEdit->setAcceptRichText(false);
    ui->chatListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    buildSelectBar();
    ui->webService->setChecked(Config::instance().webService);
    setAcceptDrops(true);
    // 窗口/托盘图标统一用多分辨率 .ico（16~256）：Qt 按当前 DPI 直接取原生尺寸，
    // 不再由单张位图缩放，小图标（标题栏 16、托盘 16/24）不会发虚
    setWindowIcon(QIcon(":/img/LANShare.ico"));
    setMinimumSize({600, 600});
    qRegisterMetaType<LFile *>("LFile");
    qRegisterMetaType<mlong>("mlong");
    qRegisterMetaType<Device>("Device");
    qRegisterMetaType<std::map<std::string, Device> >("std::map<std::string,Device>");
    qRegisterMetaType<std::string>("string");
    qRegisterMetaType<AcceptFiles *>("AcceptFiles");
    // 收到消息
    connect(this, SIGNAL(sigNewMessage(Device, QString, bool)),
            this, SLOT(newMessage(Device, QString, bool)));
    connect(this, SIGNAL(sigNewWebClient(QString, QString)),
            this, SLOT(newWebClient(QString, QString)));
    connect(this, SIGNAL(sigUpdateProgress()),
            this, SLOT(updateProgress()));
    connect(this, SIGNAL(sigReceiveFile(LFile * , QString, QString, QString, mlong, bool, bool, bool)),
            this, SLOT(receiveFile(LFile * , QString, QString, QString, mlong, bool, bool, bool)));
    connect(this, SIGNAL(sigReceiveFileProgress(QString, int)),
            this, SLOT(receiveFileProgress(QString, int)));
    connect(this, SIGNAL(sigReceiveFileSuccess(QString, bool, QString)),
            this, SLOT(receiveFileSuccess(QString, bool, QString)));
    connect(this, SIGNAL(sigCopyText(QString)),
            this, SLOT(copyText(QString)));
    connect(this, SIGNAL(sigRequestRecvFiles(AcceptFiles * )),
            this, SLOT(requestRecvFiles(AcceptFiles * )));
    loadData();
    ui->chatListWidget->scrollToBottom();
    mUtils::setFileAssociation(Config::instance().contextMenu);
    if (Config::instance().allowBackgroundRunning) {
        createTrayIcon();
    }
}


LANShareWindow::~LANShareWindow() {
    delete smoothScrollHandler;
    delete ui;
}

void LANShareWindow::on_chatListWidget_customContextMenuRequested(const QPoint &pos) {
    QListWidgetItem *item = ui->chatListWidget->itemAt(pos);
    if (item == nullptr)
        return;
    auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(item));
    if (message == nullptr)
        return;

    QMenu popMenu(this);
    QAction *openAction = nullptr;
    QAction *openDirAction = nullptr;
    QAction *resendAction = nullptr;
    if (message->hasCompletedFile()) {
        openAction = popMenu.addAction(tr("打开"));
        openDirAction = popMenu.addAction(tr("在文件夹打开"));
    }
    // 文件/图片消息可重新发送：没传完的由 V5 续传协议自动从断点继续，
    // 传完的直接全量重发
    if (message->isFileMessage()) {
        resendAction = popMenu.addAction(tr("发送"));
    }
    QAction *copyText = popMenu.addAction(tr("复制文本"));
    QAction *freeCopyText = popMenu.addAction(tr("自由复制文本"));
    QAction *deleteAction = popMenu.addAction(tr("删除"));
    QAction *multiSelectAction = popMenu.addAction(tr("多选"));

    QAction *action = popMenu.exec(QCursor::pos());
    if (action == nullptr) {
        return;
    }
    const QString filePath = message->firstFilePath();
    if (action == openAction) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    } else if (action == openDirAction) {
#if defined(PLATFORM_WINDOWS)
        QDir dir(filePath);
        QString path = dir.path();
        if (!FileUtils::exists(path)) {
            QMessageBox::warning(this, tr("警告"), tr("文件不存在或已经被删除"));
        } else {
            path.replace("/", "\\");
            QProcess::startDetached("explorer", QStringList() << "/select," << path);
        }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX)
        QProcess::startDetached("nautilus", QStringList() << filePath);
#elif defined(PLATFORM_MACOS)
#endif
    } else if (resendAction != nullptr && action == resendAction) {
        resendMessageFiles(message);
    } else if (action == copyText) {
        QApplication::clipboard()->setText(message->displayText());
    } else if (action == freeCopyText) {
        CopyableTextDialog dialog(message->displayText());
        dialog.show();
        dialog.exec();
    } else if (action == deleteAction) {
        for (LFile *lFile : message->collectFiles()) {
            if (lFile != nullptr) {
                lFile->cancelFileTransfer();
            }
        }
        MessageDB::instance().deleteMessage(message->uuid());
        ui->chatListWidget->removeItemWidget(item);
        delete item;
    } else if (action == multiSelectAction) {
        enterSelectMode(item);
    }
}

/**
 * 重新发送消息中的文件/图片
 *
 * 发送方失败消息的 filePath 记录的是源文件路径，接收完成的为本地保存路径，
 * 两者均可直接再次发送。重建的 LFile 与原文件路径、大小一致，V5 协议下
 * 稳定 fileId 也一致：接收端仍持有 .part 分段的未传完文件会从断点继续，
 * 已传完的文件（分段已落地）偏移量为 0，等价于全量重新发送。
 */
void LANShareWindow::resendMessageFiles(ChatItemWidget *message) {
    std::vector<LFile *> list;
    const ChatMessage &msg = message->message();
    for (const ContentElement &el: msg.elements) {
        if (el.type != ElementType::File && el.type != ElementType::Image) {
            continue;
        }
        QString path = el.filePath;
        if (path.isEmpty() && el.file != nullptr) {
            // 传输进行中的发送方消息尚未回写 filePath，取运行期 LFile 的源路径
            path = el.file->getPath();
        }
        QFileInfo fileInfo(path);
        if (!fileInfo.exists()) {
            continue;
        }
        auto *lFile = new LFile();
        lFile->setFileId(el.elementUuid);
        lFile->setFileName(fileInfo.fileName());
        lFile->setFileSize(fileInfo.size());
        lFile->setPath(fileInfo.filePath());
        lFile->setIsDirectory(fileInfo.isDir());
        list.push_back(lFile);
    }
    if (list.empty()) {
        QMessageBox::warning(this, tr("警告"), tr("文件不存在或已经被删除"));
        return;
    }
    showDeviceSelecter(list);
}

void LANShareWindow::on_chatListWidget_itemDoubleClicked(QListWidgetItem *item) {
    if (m_selectMode) {
        // 多选模式下双击不打开预览，只切换勾选
        if (ChatItemWidget *w = widgetOfItem(item)) {
            w->setSelected(!w->isSelected());
        }
        return;
    }
    auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(item));
    if (message == nullptr) {
        return;
    }
    if (message->isFileMessage()) {
        const QString path = message->firstFilePath();
        if (!path.isEmpty()) {
            if (ChatMessage::isImageName(path)) {
                openImagePreview(path);
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        }
    } else {
        CopyableTextDialog dialog(message->displayText());
        dialog.show();
        dialog.exec();
    }
}

void LANShareWindow::on_chatListWidget_itemClicked(QListWidgetItem *item) {
    if (m_selectMode) {
        if (ChatItemWidget *w = widgetOfItem(item)) {
            w->setSelected(!w->isSelected());
        }
    }
}

void LANShareWindow::on_setting_triggered() {
    auto *setting = new Setting(this);
    QObject::connect(setting, SIGNAL(sigUpdateSetting()), this, SLOT(updateSetting()));
    setting->show();
}

void LANShareWindow::on_actionClearAll_triggered() {
    MessageDB::instance().deleteAllMessage();
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(ui->chatListWidget->item(i)));
        if (message != nullptr) {
            for (LFile *lFile : message->collectFiles()) {
                if (lFile != nullptr) {
                    lFile->cancelFileTransfer();
                }
            }
        }
    }
    ui->chatListWidget->clear();
    // 库已清空：分页游标与“无更多”标记必须归零，否则滚到顶部不会再拉数据
    pageCount = 0;
    noMoreData = false;
}

void LANShareWindow::on_actionClearMessage_triggered() {
    std::list<QListWidgetItem *> itemList;
    MessageDB &messageDb = MessageDB::instance();
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        QListWidgetItem *item = ui->chatListWidget->item(i);
        auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(item));
        if (message != nullptr && message->isTextMessage()) {
            itemList.push_back(item);
            messageDb.deleteMessage(message->uuid());
        }
    }
    for (auto item: itemList) {
        ui->chatListWidget->removeItemWidget(item);
        delete item;
    }
}

void LANShareWindow::on_actionClearFile_triggered() {
    std::list<QListWidgetItem *> itemList;
    MessageDB &messageDb = MessageDB::instance();
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        QListWidgetItem *item = ui->chatListWidget->item(i);
        auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(item));
        if (message != nullptr && message->isFileMessage()) {
            itemList.push_back(item);
            messageDb.deleteMessage(message->uuid());
            for (LFile *lFile : message->collectFiles()) {
                if (lFile != nullptr) {
                    lFile->cancelFileTransfer();
                }
            }
        }
    }
    for (auto item: itemList) {
        ui->chatListWidget->removeItemWidget(item);
        delete item;
    }
}

void LANShareWindow::on_actionAbout_triggered() {
    auto *about = new About(this);
    about->show();
}

void LANShareWindow::on_actionScanConnect_triggered() {
    auto *dialog = new QRConnectDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void LANShareWindow::on_actionWebQrCode_triggered() {
    auto *dialog = new WebUrlQRDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void LANShareWindow::sendMessage(bool isClip) {
    QString msg = ui->textEdit->toPlainText();
    if (msg.isEmpty()) {
        return;
    }
    ui->textEdit->setText("");
    ChatMessage data = ChatMessage::fromText(msg, Config::instance().clientName, Device::L_WIN, ChatDirection::Right);
    data.uuid = Utils::getUUID();
    auto *messageW = new ChatItemWidget(data, ui->chatListWidget);
    auto *item = new QListWidgetItem(ui->chatListWidget);
    handleMessage(messageW, item);
    ui->chatListWidget->scrollToBottom();
    LANShare::getInstance()->udpProtocol.broadcastMessage(currentDevice.getDevName().isEmpty() ? nullptr : &currentDevice, msg,
                                              isClip, true);
    ui->textEdit->setFocus();
    MessageDB::instance().addMessage(messageW->message());
}

void LANShareWindow::on_sendButton_clicked() {
    sendMessage();
}

void LANShareWindow::on_selectSendDevices_clicked() {
    auto *deviceSelecter = new DeviceSelecter([this](const Device &device) {
        if (device.getDevIp().isEmpty()) {
            currentDevice = Device();
            ui->selectSendDevices->setText(tr("所有设备"));
        } else {
            currentDevice = device;
            ui->selectSendDevices->setText(device.getDevName());
            qDebug() << "选择设备:" << currentDevice.getDevName();
        }
    }, true, LANShare::getInstance(), this);
    deviceSelecter->show();
}

void LANShareWindow::on_selectFiles_clicked() {
    // 创建一个文件对话框
    QFileDialog fileDialog;
    // 设置对话框的标题
    fileDialog.setWindowTitle("选择多个文件");
    // 设置对话框可以选择多个文件
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    // 打开文件对话框
    if (fileDialog.exec()) {
        // 获取用户选择的文件列表
        QStringList selectedFiles = fileDialog.selectedFiles();
        std::vector<LFile *> list;
        // 打印选择的文件路径
        qDebug() << "Selected Files:";
        for (const QString &file: selectedFiles) {
            QFileInfo fileInfo(file);
            auto *lFile = new LFile();
            lFile->setFileName(fileInfo.fileName());
            lFile->setFileSize(fileInfo.size());
            lFile->setPath(fileInfo.filePath());
            lFile->setIsDirectory(fileInfo.isDir());
            list.push_back(lFile);
            qDebug() << file;
        }
        showDeviceSelecter(list);
    }
}

void LANShareWindow::updateWebServiceIp() {
    std::vector<Device> mDevices = LANShare::getInstance()->getMDevices();
    if (Config::instance().webService && !mDevices.empty()) {
        ui->web_address->setText("http://" + mDevices[0].getDevIp() + ":" + QString::number(Config::instance().tcpPort));
        ui->web_address->show();
    } else {
        ui->web_address->setText("");
        ui->web_address->hide();
    }
}

void LANShareWindow::on_webService_clicked() {
    Config::instance().webService = ui->webService->isChecked();
    QSettings *settings = Config::instance().getSettings();
    settings->setValue(WEB_SERVICE, Config::instance().webService);
    updateWebServiceIp();
}

void LANShareWindow::on_selectDirectories_clicked() {
    // 创建一个文件对话框
    QFileDialog fileDialog;
    // 设置对话框的标题
    fileDialog.setWindowTitle("选择多个文件");
    // 设置对话框可以选择多个文件
    fileDialog.setFileMode(QFileDialog::Directory);
    // 打开文件对话框
    if (fileDialog.exec()) {
        // 获取用户选择的文件列表
        QStringList selectedFiles = fileDialog.selectedFiles();
        std::vector<LFile *> list;
        // 打印选择的文件路径
        qDebug() << "Selected Files:";
        for (const QString &file: selectedFiles) {
            QFileInfo fileInfo(file);
            auto *lFile = new LFile();
            lFile->setFileName(fileInfo.fileName());
            lFile->setFileSize(fileInfo.size());
            lFile->setPath(fileInfo.filePath());
            lFile->setIsDirectory(fileInfo.isDir());
            list.push_back(lFile);
            qDebug() << file;
        }
        showDeviceSelecter(list);
    }
}

void LANShareWindow::newMessage(Device device, QString message, bool isLeft) {
    ChatMessage data = ChatMessage::fromText(
        message, device.getDevName(), device.getDevMode(),
        isLeft ? ChatDirection::Left : ChatDirection::Right);
    data.uuid = Utils::getUUID();
    auto *messageW = new ChatItemWidget(data, ui->chatListWidget);
    auto *item = new QListWidgetItem(ui->chatListWidget);
    handleMessage(messageW, item);
    MessageDB::instance().addMessage(messageW->message());
    ui->chatListWidget->scrollToBottom();
}

void LANShareWindow::newWebClient(QString token, QString ip) {
    // 创建一个消息框
    QMessageBox msgBox(this);
    msgBox.setText("是否同意网页客户端：【" + ip + "】的访问请求？");
    msgBox.setWindowTitle("有新的客户端访问");
    msgBox.setWindowIcon(QIcon(":/img/LANShare.ico"));
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setWindowFlags(msgBox.windowFlags() | Qt::WindowStaysOnTopHint); // 设置窗口始终显示在顶层
    msgBox.setButtonText(QMessageBox::No, "不同意");
    msgBox.setButtonText(QMessageBox::Yes, "同意");
    // 显示消息框，并获取用户的选择
    int ret = msgBox.exec();
    TokenDBUtil &tokenDBUtil = TokenDBUtil::instance();
    // 根据用户的选择进行相应的操作
    if (ret == QMessageBox::Yes) {
        tokenDBUtil.addToken(token, false, ip);
    }
}

//当用户拖动文件到主窗口时候，就会触发dragEnterEvent事件
void LANShareWindow::dragEnterEvent(QDragEnterEvent *event) {
    event->acceptProposedAction();
}

// 拖拽文件到窗口事件
void LANShareWindow::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();
    std::vector<LFile *> list;
    for (const auto &url: urls) {
        QFileInfo fileInfo(url.toLocalFile()); //绝对路径与相对路径都可以
        auto *lFile = new LFile();
        lFile->setFileName(fileInfo.fileName());
        lFile->setFileSize(fileInfo.size());
        lFile->setPath(fileInfo.filePath());
        lFile->setIsDirectory(fileInfo.isDir());
        list.push_back(lFile);
    }
    showDeviceSelecter(list);
}

void LANShareWindow::handleMessage(ChatItemWidget *messageW, QListWidgetItem *item) {
    connect(messageW, &ChatItemWidget::imageRequested, this, &LANShareWindow::openImagePreview);
    connect(messageW, &ChatItemWidget::selectionChanged, this, &LANShareWindow::refreshSelectionState);
    messageW->setCheckMode(m_selectMode);
    // 只设一次 sizeHint：重复设置会让 QListWidget 再跑一轮项布局
    item->setSizeHint(messageW->sizeHint());
    ui->chatListWidget->addItem(item);
    ui->chatListWidget->setItemWidget(item, messageW);
}

void LANShareWindow::insertMessage(ChatItemWidget *messageW, QListWidgetItem *item) {
    connect(messageW, &ChatItemWidget::imageRequested, this, &LANShareWindow::openImagePreview);
    connect(messageW, &ChatItemWidget::selectionChanged, this, &LANShareWindow::refreshSelectionState);
    messageW->setCheckMode(m_selectMode);
    item->setSizeHint(messageW->sizeHint());
    ui->chatListWidget->insertItem(0, item);
    ui->chatListWidget->setItemWidget(item, messageW);
}

void LANShareWindow::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event)
    ui->textEdit->resize(this->width() - 20, ui->widget->height() - 20);
    ui->textEdit->move(10, 10);
    ui->sendButton->move(ui->textEdit->width() + ui->textEdit->x() - ui->sendButton->width() - 10,
                         ui->textEdit->height() + ui->textEdit->y() - ui->sendButton->height() - 10);
}


void LANShareWindow::onScroll(int value) {
    Q_UNUSED(value)
    QScrollBar *bar = ui->chatListWidget->verticalScrollBar();
    if (bar->value() > bar->minimum() || noMoreData || loadingData || pendingLoad) {
        return;
    }
    // 推到下一轮事件循环再建页：滚动（或惯性逐帧 setValue）过程中同步查库、
    // 解密、构控件，会把正在滚动的这几帧全部拖住
    pendingLoad = true;
    QTimer::singleShot(0, this, [this] {
        pendingLoad = false;
        loadData();
    });
}

void LANShareWindow::openImagePreview(const QString &path) {
    // 多选模式下不弹预览，避免与选择操作冲突
    if (m_selectMode) {
        return;
    }
    // 未开启内置预览器时，直接用系统默认程序打开这一张图
    if (!Config::instance().useInternalImagePreview) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return;
    }
    // 汇聚整段会话中已下载完成的图片（按出现顺序去重），作为预览窗口的浏览列表
    QStringList images;
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        auto *w = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(ui->chatListWidget->item(i)));
        if (w == nullptr) {
            continue;
        }
        const ChatMessage &m = w->message();
        for (const ContentElement &el: m.elements) {
            if (el.type == ElementType::Image && el.completed && !el.filePath.isEmpty()
                && FileUtils::exists(el.filePath) && !images.contains(el.filePath)) {
                images << el.filePath;
            }
        }
    }
    int idx = images.indexOf(path);
    if (idx < 0) {
        // 列表里没有（如点击的瞬间尚未入库），至少保证能预览这一张
        images.prepend(path);
        idx = 0;
    }
    auto *win = new ImagePreview(images, idx, this);
    win->setAttribute(Qt::WA_DeleteOnClose);
    win->show();
    win->raise();
    win->activateWindow();
}

ChatItemWidget *LANShareWindow::widgetOfItem(QListWidgetItem *item) const {
    if (item == nullptr) {
        return nullptr;
    }
    return qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(item));
}

void LANShareWindow::buildSelectBar() {
    // 顶部多选操作栏：平时隐藏。用一层透明外框提供四周留白，
    // 内层 #selectBar 是圆角工具条本体；局部样式表覆盖全局按钮风格，
    // 避免继承成大块蓝色按钮。
    m_selectBar = new QWidget;
    // 锁定为内容自然高度：默认 Preferred 会与聊天列表平分竖向空间，把列表挤到只剩几条
    m_selectBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *hostLay = new QHBoxLayout(m_selectBar);
    hostLay->setContentsMargins(10, 6, 10, 2);
    hostLay->setSpacing(0);

    auto *bar = new QWidget(m_selectBar);
    bar->setObjectName("selectBar");
    bar->setStyleSheet(
            "#selectBar{ background: rgba(128,128,134,30); border-radius: 12px; }"
            "#selectBar QLabel, #selectBar QCheckBox{ color: palette(text); font-size: 13px; }"
            "#selectBar QLabel#selCount{"
            "  background: rgba(128,128,134,55); border-radius: 10px;"
            "  padding: 2px 10px; font-size: 12px; }"
            "#selectBar QPushButton{"
            "  background: transparent; color: palette(text);"
            "  border: 1px solid rgba(128,128,134,80); border-radius: 15px;"
            "  padding: 5px 16px; font-size: 13px; }"
            "#selectBar QPushButton:hover{ background: rgba(128,128,134,45); }"
            "#selectBar QPushButton:pressed{ background: rgba(128,128,134,80); }"
            "#selectBar QPushButton#delSelBtn{ color: #E53935; border-color: rgba(229,57,53,110); }"
            "#selectBar QPushButton#delSelBtn:hover{ background: rgba(229,57,53,30); }"
            "#selectBar QPushButton#delSelBtn:pressed{ background: rgba(229,57,53,60); }");
    hostLay->addWidget(bar);

    auto *lay = new QHBoxLayout(bar);
    lay->setContentsMargins(14, 4, 10, 4);
    lay->setSpacing(8);

    m_selectAllCheck = new QCheckBox(tr("全选"), bar);
    m_selectAllCheck->setCursor(Qt::PointingHandCursor);
    lay->addWidget(m_selectAllCheck);

    m_selectCount = new QLabel(tr("已选 0 项"), bar);
    m_selectCount->setObjectName("selCount");
    lay->addWidget(m_selectCount);

    lay->addStretch();

    auto *delBtn = new QPushButton(tr("删除所选"), bar);
    delBtn->setObjectName("delSelBtn");
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setFocusPolicy(Qt::NoFocus);
    connect(delBtn, &QPushButton::clicked, this, &LANShareWindow::deleteSelectedMessages);
    lay->addWidget(delBtn);

    auto *exitBtn = new QPushButton(tr("完成"), bar);
    exitBtn->setObjectName("exitSelBtn");
    exitBtn->setCursor(Qt::PointingHandCursor);
    exitBtn->setFocusPolicy(Qt::NoFocus);
    connect(exitBtn, &QPushButton::clicked, this, &LANShareWindow::exitSelectMode);
    lay->addWidget(exitBtn);

    connect(m_selectAllCheck, &QCheckBox::toggled, this, &LANShareWindow::onSelectAllToggled);

    m_selectBar->hide();
    ui->verticalLayout->insertWidget(0, m_selectBar);
}

void LANShareWindow::enterSelectMode(QListWidgetItem *preselect) {
    if (m_selectMode) {
        return;
    }
    m_selectMode = true;
    for (int i = 0; i < ui->chatListWidget->count(); ++i) {
        if (ChatItemWidget *w = widgetOfItem(ui->chatListWidget->item(i))) {
            w->setCheckMode(true);
        }
    }
    if (ChatItemWidget *w = widgetOfItem(preselect)) {
        w->setSelected(true);
    }
    m_selectBar->show();
    refreshSelectionState();
}

void LANShareWindow::exitSelectMode() {
    if (!m_selectMode) {
        return;
    }
    m_selectMode = false;
    {
        QSignalBlocker blocker(m_selectAllCheck);
        m_selectAllCheck->setChecked(false);
    }
    for (int i = 0; i < ui->chatListWidget->count(); ++i) {
        if (ChatItemWidget *w = widgetOfItem(ui->chatListWidget->item(i))) {
            w->setCheckMode(false);
        }
    }
    m_selectBar->hide();
}

void LANShareWindow::onSelectAllToggled(bool checked) {
    if (!m_selectMode) {
        return;
    }
    for (int i = 0; i < ui->chatListWidget->count(); ++i) {
        if (ChatItemWidget *w = widgetOfItem(ui->chatListWidget->item(i))) {
            w->setSelected(checked);
        }
    }
    refreshSelectionState();
}

void LANShareWindow::refreshSelectionState() {
    if (!m_selectMode || m_selectCount == nullptr) {
        return;
    }
    const int total = ui->chatListWidget->count();
    int sel = 0;
    for (int i = 0; i < total; ++i) {
        if (ChatItemWidget *w = widgetOfItem(ui->chatListWidget->item(i))) {
            if (w->isSelected()) {
                ++sel;
            }
        }
    }
    m_selectCount->setText(tr("已选 %1 项").arg(sel));
    const bool all = total > 0 && sel == total;
    {
        QSignalBlocker blocker(m_selectAllCheck);
        m_selectAllCheck->setChecked(all);
    }
}

void LANShareWindow::deleteSelectedMessages() {
    if (!m_selectMode) {
        return;
    }
    int sel = 0;
    for (int i = 0; i < ui->chatListWidget->count(); ++i) {
        if (ChatItemWidget *w = widgetOfItem(ui->chatListWidget->item(i))) {
            if (w->isSelected()) {
                ++sel;
            }
        }
    }
    if (sel == 0) {
        return;
    }
    if (QMessageBox::question(this, tr("删除"),
                              tr("确定删除选中的 %1 条消息？").arg(sel)) != QMessageBox::Yes) {
        return;
    }
    // 倒序遍历：删除当前项不会影响尚未访问的下标
    for (int i = ui->chatListWidget->count() - 1; i >= 0; --i) {
        QListWidgetItem *item = ui->chatListWidget->item(i);
        ChatItemWidget *w = widgetOfItem(item);
        if (w == nullptr || !w->isSelected()) {
            continue;
        }
        for (LFile *lFile : w->collectFiles()) {
            if (lFile != nullptr) {
                lFile->cancelFileTransfer();
            }
        }
        MessageDB::instance().deleteMessage(w->uuid());
        ui->chatListWidget->removeItemWidget(item);
        delete item;
    }
    refreshSelectionState();
}

bool LANShareWindow::getClipboard() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    if (mimeData->hasImage()) {
        auto image = qvariant_cast<QImage>(mimeData->imageData());
        if (!image.isNull()) {
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly); 
            // 将图像转换为 JPEG 格式的字节数组
            image.save(&buffer, "PNG");
            std::vector<LFile *> list;
            auto *lFile = new LFile();
            lFile->setFileName(Utils::getUUID() + ".png");
            lFile->setFileSize(byteArray.size());
            lFile->setByteArray(byteArray);
            lFile->setIsDirectory(false);
            lFile->setFileType(FILE_IMAGE);
            list.push_back(lFile);
            showDeviceSelecter(list);
        }
    } else if (mimeData->hasUrls()) {
        QString text = mimeData->text();
        qDebug() << "Clipboard contains text: " << text;
        QList<QUrl> urlList = mimeData->urls();
        qDebug() << "Clipboard contains URLs:" << urlList.size();
        std::vector<LFile *> list;
        for (const QUrl &url: urlList) {
            if (url.isLocalFile()) {
                qDebug() << url.toLocalFile();
                QFileInfo fileInfo(url.toLocalFile()); //绝对路径与相对路径都可以
                auto *lFile = new LFile();
                lFile->setFileName(fileInfo.fileName());
                lFile->setFileSize(fileInfo.size());
                lFile->setPath(fileInfo.filePath());
                lFile->setIsDirectory(fileInfo.isDir());
                list.push_back(lFile);
            } else {
                qDebug() << "path: " << url.path();
                ui->textEdit->setPlainText(ui->textEdit->toPlainText() + url.path());
                QTextCursor cursor = ui->textEdit->textCursor();
                cursor.movePosition(QTextCursor::End);
                ui->textEdit->setTextCursor(cursor);
            }
        }
        if (!list.empty()) {
            showDeviceSelecter(list);
        }
    } else if (mimeData->hasText() || mimeData->hasHtml()) {
        QString text = mimeData->text();
        ui->textEdit->setPlainText(ui->textEdit->toPlainText() + text);
        QTextCursor cursor = ui->textEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        ui->textEdit->setTextCursor(cursor);
    }
    return true;
}

void LANShareWindow::keyPressEvent(QKeyEvent *event) {
    if (m_selectMode && event->key() == Qt::Key_Escape) {
        exitSelectMode();
        return;
    }
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
        if (keyEvent->modifiers() & Qt::ControlModifier && keyEvent->key() == Qt::Key_V) {
            getClipboard();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

bool LANShareWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->textEdit) {
        if (event->type() == QEvent::Clipboard) {
            auto *textEdit = qobject_cast<QTextEdit *>(watched);
            QClipboard *clipboard = QGuiApplication::clipboard();
            const QMimeData *mimeData = clipboard->mimeData();
            if (mimeData->hasImage()) {
                auto image = qvariant_cast<QImage>(mimeData->imageData());
                if (!image.isNull()) {
                    QTextCursor cursor = textEdit->textCursor();
                    cursor.insertImage(image);
                    textEdit->setTextCursor(cursor);
                    return true;
                }
            }
        } else if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
            if (keyEvent->modifiers() & Qt::ControlModifier && keyEvent->key() == Qt::Key_V) {
                getClipboard();
                return true;
            } else if (keyEvent->modifiers() & Qt::ShiftModifier && keyEvent->key() == Qt::Key_Return) {
                // 获取当前光标位置
                QTextCursor cursor = ui->textEdit->textCursor();
                int cursorPosition = cursor.position();
                // 获取光标位置前后的文本
                QString textBeforeCursor = ui->textEdit->toPlainText().left(cursorPosition);
                QString textAfterCursor = ui->textEdit->toPlainText().mid(cursorPosition);
                // 在光标位置插入换行符
                ui->textEdit->setPlainText(textBeforeCursor + "\n" + textAfterCursor);
                // 移动光标到换行符后
                cursor.setPosition(cursorPosition + 1);
                ui->textEdit->setTextCursor(cursor);
                return true;
            } else if (keyEvent->modifiers() & Qt::ControlModifier && keyEvent->key() == Qt::Key_Return) {
                sendMessage(true);
                return true;
            } else if (keyEvent->key() == Qt::Key_Return) {
                sendMessage(false);
                return true;
            }
        }
    } /*else if (watched == ui->chatListWidget) {
        qDebug("angleDelta %d",event->type());
        if (event->type() == QEvent::Wheel) {
            auto *wheelEvent = dynamic_cast<QWheelEvent *>(event);
            if (wheelEvent->angleDelta().y() > 0) {
                // 向上滚动
                if (ui->chatListWidget->verticalScrollBar()->value() ==
                    ui->chatListWidget->verticalScrollBar()->minimum()) {
                    qDebug("top");
                    loadData();
                }
            }
            return true;
        }else if (event->type() == QEvent::ToolTip) {
            return true;
        }
    }*/
    return QMainWindow::eventFilter(watched, event);
}

void LANShareWindow::updateProgress() {
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(ui->chatListWidget->item(i)));
        if (message != nullptr) {
            message->resetAllProgress();
        }
    }
    ui->chatListWidget->viewport()->update();
}

void LANShareWindow::receiveFile(LFile *lFile, const QString &uuid, QString message, QString userName, mlong fileSize,
                                 bool left, bool isFile, bool completed) {
    const ChatDirection dir = left ? ChatDirection::Left : ChatDirection::Right;
    // V5 链路 uuid 取自稳定 fileId：重发同一文件时命中已有消息，
    // 复用原气泡重置为传输中状态，进度/结果会直接刷回老气泡，
    // 列表不再堆积重复条目，也不产生主键冲突的新插入
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        auto *existing = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(ui->chatListWidget->item(i)));
        if (existing == nullptr || existing->uuid() != uuid) {
            continue;
        }
        if (existing->restartElement(uuid, lFile, fileSize, QString::fromStdString(Utils::computeSize(fileSize)))) {
            MessageDB::instance().updateMessage(existing->message());
            existing->update();
            ui->chatListWidget->scrollToBottom();
            return;
        }
        break;
    }
    // 图片文件（发送侧标记 FILE_IMAGE 或按扩展名判定）改用 Image 元素，
    // 消息列表直接展示缩略图；所有传输链路都汇聚到本槽，网络层无需改动
    const bool isImage = (lFile != nullptr && lFile->getFileType() == FILE_IMAGE)
                         || ChatMessage::isImageName(message);
    ChatMessage data = isImage
                           ? ChatMessage::fromImage(QString(), message, uuid, userName,
                                                    Device::L_WIN, dir, fileSize)
                           : ChatMessage::fromFile(message, uuid, userName, Device::L_WIN,
                                                   dir, fileSize, !isFile);
    data.uuid = uuid;
    ContentElement *el = isImage ? data.firstOf(ElementType::Image) : data.firstOf(ElementType::File);
    if (el != nullptr) {
        el->file = lFile;
        el->sizeStr = QString::fromStdString(Utils::computeSize(fileSize));
        // 发送方的 LFile 持有源文件路径，创建气泡时就写入并落库：
        // 若等传输结束才回写，一旦断连/异常退出没有 success 回调，
        // DB 里 filePath 为空，右键重新发送会误报“文件不存在”；
        // 接收方此时 path 尚未落地为空串，不受影响
        if (lFile != nullptr && el->filePath.isEmpty()) {
            el->filePath = lFile->getPath();
        }
        if (completed) {
            el->completed = true;
            el->received = true;
            el->progress = 100;
            if (isImage && lFile != nullptr && !lFile->getPath().isEmpty()) {
                el->filePath = lFile->getPath();
            }
        }
    }
    auto *messageW = new ChatItemWidget(data, ui->chatListWidget);
    auto *item = new QListWidgetItem(ui->chatListWidget);
    handleMessage(messageW, item);
    MessageDB::instance().addMessage(messageW->message());
    ui->chatListWidget->scrollToBottom();
}

void LANShareWindow::receiveFileProgress(const QString &uuid, int progress) {
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(ui->chatListWidget->item(i)));
        if (message != nullptr && message->setElementProgress(uuid, progress)) {
            // 只重绘这一条消息：进度回调按数据块频率触发，
            // 整个 viewport 重绘会在传输期间把滚动拖成连帧丢帧
            message->update();
            persistProgressThrottled(message, uuid, progress);
            break;
        }
    }
}

void LANShareWindow::receiveFileSuccess(const QString &uuid, bool completed, const QString &filePath) {
    MessageDB &messageDb = MessageDB::instance();
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(ui->chatListWidget->item(i)));
        if (message == nullptr) {
            continue;
        }
        if (message->setElementResult(uuid, completed, filePath, true)) {
            message->clearFilePointers();
            messageDb.updateMessage(message->message());
            g_lastSavedProgress.remove(uuid);
            message->update();
            break;
        }
    }
}

void LANShareWindow::updateSetting() {
    qDebug() << "updateSetting";
    LANShare::getInstance()->updateMDevices();
    mUtils::setFileAssociation(Config::instance().contextMenu);
    ui->webService->setChecked(Config::instance().webService);
    if (Config::instance().allowBackgroundRunning) {
        createTrayIcon();
    } else {
        closeTrayIcon();
    }
}

void LANShareWindow::loadDeviceList() {
    qDebug() << "点击";
}

void LANShareWindow::closeEvent(QCloseEvent *event) {
    if (Config::instance().allowBackgroundRunning) {
        // 取消默认的关闭操作
        event->ignore();
        // 隐藏窗口
        this->hide();
    } else {
        QApplication::quit();
    }
}

void LANShareWindow::copyText(const QString &text) {
    QClipboard *clip = QApplication::clipboard();
    clip->setText(text, QClipboard::Clipboard);
}

// Set window to top layer
void LANShareWindow::setWindowToTopLayer() {
    if (this->isMinimized()) {
        this->showNormal();
    }
#if defined(PLATFORM_WINDOWS)
    //设置窗口置顶
    ::SetWindowPos(HWND(this->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    ::SetWindowPos(HWND(this->winId()), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#endif
    this->raise();
    this->show();
    this->activateWindow();
}

LANShareWindow *LANShareWindow::getInstance() {
    return lanShareWindow;
}

void LANShareWindow::requestRecvFiles(AcceptFiles *acceptFiles) {
    QMessageBox msgBox(this);
    msgBox.setText(QString(tr("是否接收设备：【%1】发送的%2个文件")).arg(acceptFiles->device.getDevName(), QString::number(acceptFiles->files.size())));
    msgBox.setWindowTitle(tr("接收文件请求"));
    msgBox.setWindowIcon(QIcon(":/img/LANShare.ico"));
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setWindowFlags(msgBox.windowFlags() | Qt::WindowStaysOnTopHint); // 设置窗口始终显示在顶层
    msgBox.setButtonText(QMessageBox::No, tr("拒绝"));
    msgBox.setButtonText(QMessageBox::Yes, tr("接收"));
    int ret = msgBox.exec();
    std::thread([ret, acceptFiles]() mutable {
        // V5 断点续传：路由到 FileServerV5（同意时回传续传偏移量表）
        if (acceptFiles->resume) {
            LANShare::getInstance()->fileServerV5.startReceiveFile(
                acceptFiles->fileTransfer,
                acceptFiles->files,
                acceptFiles->stream,
                acceptFiles->needEncData,
                ret == QMessageBox::Yes
            );
        }
        // 适配旧版本
        else if (acceptFiles->device.getDataVersion() < DATA_VERSION_4) {
            LANShare::getInstance()->legacyFileTransfer.startHandleRecvFile(
                ret == QMessageBox::Yes,
                acceptFiles->device,
                acceptFiles->needEncData,
                acceptFiles->files,
                std::move(acceptFiles->tcpClient)
            );
        } else {
            LANShare::getInstance()->legacyFileTransfer.startNewVersionHandleRecvFile(
                acceptFiles->device,
                acceptFiles->fileTransfer,
                acceptFiles->files,
                acceptFiles->stream,
                acceptFiles->needEncData,
                ret == QMessageBox::Yes
            );
        }

        delete acceptFiles;
    }).detach();
}

void LANShareWindow::closeTrayIcon() {
    if (trayIcon != nullptr) {
        trayIcon->deleteLater();
        delete trayIcon;
        trayIcon = nullptr;
    }
}

void LANShareWindow::createTrayIcon() {
    // 检查系统是否支持托盘图标
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug("系统不支持托盘图标！");
        return;
    }
    if (trayIcon != nullptr) {
        return;
    }
    // 创建系统托盘图标
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/img/LANShare.ico"));
    qDebug() << "Tray icon created";
    // 创建托盘菜单
    auto *trayMenu = new QMenu();
    auto *show = new QAction("显示窗口", this);
    auto *quitAction = new QAction("退出", this);
    QObject::connect(show, &QAction::triggered, [this]() {
        setWindowToTopLayer();
    });
    QObject::connect(quitAction, &QAction::triggered, [this]() {
        QApplication::quit();
    });
    // 托盘图标双击事件
    QObject::connect(trayIcon, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            setWindowToTopLayer();
        }
    });
    trayMenu->addAction(show);
    trayMenu->addAction(quitAction);
    trayIcon->setContextMenu(trayMenu);
    // 显示托盘图标
    trayIcon->show();
    //    trayIcon->showMessage("Notification Title", "This is the notification message.", QSystemTrayIcon::Information,
    //                          3000);
}

void LANShareWindow::loadData() {
    if (loadingData || noMoreData) {
        return;
    }
    loadingData = true;
    MessageDB &messageDb = MessageDB::instance();
    std::list<ChatMessage> data = messageDb.queryList(pageSize, pageCount++);
    // 只有“一页都拉不到”才代表更早的消息已穷尽：
    // 最近一页本来就常不满 pageSize（总数 % pageSize），用“不满一页”判定
    // 会在首次建页就误锁 noMoreData，导致滚到顶部再也不加载下一页
    if (data.empty()) {
        noMoreData = true;
        loadingData = false;
        return;
    }
    QScrollBar *bar = ui->chatListWidget->verticalScrollBar();
    // 先停掉滚动惯性：否则惯性下一帧会用旧目标位置覆盖下面的恢复（"弹跳"根因）
    if (smoothScrollHandler != nullptr) {
        smoothScrollHandler->cancel();
    }
    // ScrollPerPixel 下 value 即像素偏移：顶部插入 N 像素内容后，
    // 把 value 平移 N 即可像素级保持画面不动（比 scrollToItem 精确且无跳变）
    const int valueBefore = bar->value();
    int addedHeight = 0;
    // 批量插入期间冻结重绘：避免每条 insertMessage 都触发全列表重排+重绘
    // （插入过程中的 valueChanged 由 loadingData 拦住，不会递归建页；
    // 这里不能 blockSignals，否则 QAbstractScrollArea 也收不到值变化，
    // 视口不会真正平移，恢复位置后画面会与滚动条错位）
    ui->chatListWidget->setUpdatesEnabled(false);
    for (auto rit = data.rbegin(); rit != data.rend(); ++rit) {
        auto *messageW = new ChatItemWidget(*rit, ui->chatListWidget);
        auto *item = new QListWidgetItem();
        insertMessage(messageW, item);
        addedHeight += item->sizeHint().height();
    }
    // 强制完成延迟布局，确保 scrollbar 范围已更新再恢复位置
    ui->chatListWidget->doItemsLayout();
    bar->setValue(valueBefore + addedHeight);
    ui->chatListWidget->setUpdatesEnabled(true);
    loadingData = false;
    // 内容还填不满视口时，根本不会产生“滚到顶部”这个 valueChanged（历史很短
    // 或单页高度不够），直接补下一页，直到出现可滚动范围或数据穷尽。
    // addedHeight > 0 是兜底保护：万一某页行高为 0，不能无限自旋补页
    if (addedHeight > 0 && !noMoreData && bar->maximum() <= bar->minimum()) {
        pendingLoad = true;
        QTimer::singleShot(0, this, [this] {
            pendingLoad = false;
            loadData();
        });
    }
}
