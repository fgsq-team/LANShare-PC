#include <QTextBlock>
#include <QGridLayout>
#include <QFileDialog>
#include <QDateTime>
#include <QDesktopServices>
#include <QIcon>
#include <QUuid>
#include <QScrollBar>

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

LANShareWindow *lanShareWindow = nullptr;

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
    ui->chatListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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
    ui->webService->setChecked(Config::instance().webService);
    setAcceptDrops(true);
    setWindowIcon(QIcon(":/img/ic_launcher.png"));
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
    if (message->hasCompletedFile()) {
        openAction = popMenu.addAction(tr("打开"));
        openDirAction = popMenu.addAction(tr("在文件夹打开"));
    }
    QAction *copyText = popMenu.addAction(tr("复制文本"));
    QAction *freeCopyText = popMenu.addAction(tr("自由复制文本"));
    QAction *deleteAction = popMenu.addAction(tr("删除"));

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
    }
}

void LANShareWindow::on_chatListWidget_itemDoubleClicked(QListWidgetItem *item) {
    auto *message = qobject_cast<ChatItemWidget *>(ui->chatListWidget->itemWidget(item));
    if (message == nullptr) {
        return;
    }
    if (message->isFileMessage()) {
        const QString path = message->firstFilePath();
        if (!path.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    } else {
        CopyableTextDialog dialog(message->displayText());
        dialog.show();
        dialog.exec();
    }
}

void LANShareWindow::on_chatListWidget_itemClicked(QListWidgetItem *item) {
    Q_UNUSED(item)
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
    msgBox.setWindowIcon(QIcon(":/img/ic_launcher.png"));
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
    item->setSizeHint(messageW->sizeHint());
    ui->chatListWidget->addItem(item);
    ui->chatListWidget->setItemWidget(item, messageW);
    item->setSizeHint(messageW->sizeHint());
}

void LANShareWindow::insertMessage(ChatItemWidget *messageW, QListWidgetItem *item) {
    item->setSizeHint(messageW->sizeHint());
    ui->chatListWidget->insertItem(0, item);
    ui->chatListWidget->setItemWidget(item, messageW);
    item->setSizeHint(messageW->sizeHint());
}

void LANShareWindow::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event)
    ui->textEdit->resize(this->width() - 20, ui->widget->height() - 20);
    ui->textEdit->move(10, 10);
    ui->sendButton->move(ui->textEdit->width() + ui->textEdit->x() - ui->sendButton->width() - 10,
                         ui->textEdit->height() + ui->textEdit->y() - ui->sendButton->height() - 10);
}


void LANShareWindow::onScroll(int value) {
    // qDebug() << "Scroll value changed to:" << value;
    if (value == 0) {
        // 向上滚动
        if (ui->chatListWidget->verticalScrollBar()->value() ==
            ui->chatListWidget->verticalScrollBar()->minimum()) {
            qDebug("top");
            loadData();
        }
    }
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
            ui->chatListWidget->viewport()->update();
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
            ui->chatListWidget->viewport()->update();
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
    msgBox.setWindowIcon(QIcon(":/img/ic_launcher.png"));
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
    trayIcon->setIcon(QIcon(":/img/ic_launcher.png"));
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
    if (loadingData) {
        return;
    }
    loadingData = true;
    MessageDB &messageDb = MessageDB::instance();
    std::list<ChatMessage> data = messageDb.queryList(pageSize, pageCount++);
    if (data.empty()) {
        loadingData = false;
        return;
    }
    QScrollBar *bar = ui->chatListWidget->verticalScrollBar();
    // 先停掉缓动动画：否则动画 tick 会用旧目标位置覆盖下面的恢复（"弹跳"根因）
    if (smoothScrollHandler != nullptr) {
        smoothScrollHandler->cancel();
    }
    // ScrollPerPixel 下 value 即像素偏移：顶部插入 N 像素内容后，
    // 把 value 平移 N 即可像素级保持画面不动（比 scrollToItem 精确且无跳变）
    const int valueBefore = bar->value();
    int addedHeight = 0;
    // 批量插入期间冻结重绘：避免每条 insertMessage 都触发全列表重排+重绘
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
}
