#include <QTextBlock>
#include <QFileDialog>
#include <QDateTime>
#include <QDesktopServices>
#include <QIcon>
#include <QUuid>

#include <qmimedata.h>
#include <vector>
#include <utility>
#include <QMessageBox>
#include "LANShareWindow.h"

#include <TimeTools.h>

#include "ui_LANShareWindow.h"
#include "Setting.h"
#include "MessageFile.h"
#include "Utils.h"
#include "DeviceSelecter.h"
#include "About.h"
#include "CopyableTextDialog.h"
#include "EditTextEventFilter.h"
#include "LANShare.h"
#include "MessageTime.h"

LANShareWindow *lanShareWindow = nullptr;

void LANShareWindow::showDeviceSelecter(std::vector<LFile *> &fileaPaths) {
    auto *deviceSelecter = new DeviceSelecter([fileaPaths](const Device &device) {
        std::thread tSendfile(LANShare::sendFile, device, fileaPaths, fileaPaths.size());
        tSendfile.detach();
    }, false, LANShare::getInstance(), this);
    deviceSelecter->show();
}

LANShareWindow::LANShareWindow(QWidget *parent) : QMainWindow(parent),
                                                  ui(new Ui::LANShareWindow) {
    lanShareWindow = this;
    networkAccessManager = new QNetworkAccessManager();
    checkVersion();
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
    ui->webService->setChecked(config.webService);
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
    connect(this, SIGNAL(sigNewWebClient(QString, QString, QString)),
            this, SLOT(newWebClient(QString, QString, QString)));
    connect(this, SIGNAL(sigUpdateProgress()),
            this, SLOT(updateProgress()));
    connect(this, SIGNAL(sigRecviceFile(LFile * , QString, QString, QString, mlong, bool, bool, bool)),
            this, SLOT(recviceFile(LFile * , QString, QString, QString, mlong, bool, bool, bool)));
    connect(this, SIGNAL(sigRecviceFileProgress(QString, int)),
            this, SLOT(recviceFileProgress(QString, int)));
    connect(this, SIGNAL(sigRecviceFileSuccess(QString, bool, QString)),
            this, SLOT(recviceFileSuccess(QString, bool, QString)));
    connect(this, SIGNAL(sigCopyText(QString)),
            this, SLOT(copyText(QString)));
    connect(this, SIGNAL(sigRequstRecvFiles(AcceptFiles * )),
            this, SLOT(requstRecvFiles(AcceptFiles * )));
    MessageDB_V3 &messageDb = MessageDB_V3::instance();
    lastMessageTime = messageDb.getLastMessageTime();
    loadData();
    ui->chatListWidget->scrollToBottom();
    connect(ui->chatListWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, &LANShareWindow::onScroll);
    mUtils::setFileAssociation(config.contextMenu);
    if (config.allowBackgroundRunning) {
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
    auto *message = (Message *) ui->chatListWidget->itemWidget(item);
    if (typeid(*message) == typeid(MessageTime)) {
        return;
    }
    auto *popMenu = new QMenu(this);
    auto *openAction = new QAction(tr("打开"), this);
    auto *openDirAction = new QAction(tr("在文件夹打开"), this);
    auto *copyText = new QAction(tr("复制文本"), this);
    auto *freeCopyText = new QAction(tr("自由复制文本"), this);
    auto *deleteAction = new QAction(tr("删除"), this);
    auto *messageFile = static_cast<MessageFile *>(message);
    if (typeid(*message) == typeid(MessageFile) && messageFile->isCompleted()) {
        popMenu->addAction(openAction);
        popMenu->addAction(openDirAction);
        popMenu->addAction(copyText);
        popMenu->addAction(freeCopyText);
        popMenu->addAction(deleteAction);
    } else {
        popMenu->addAction(copyText);
        popMenu->addAction(freeCopyText);
        popMenu->addAction(deleteAction);
    }
    QAction *action = popMenu->exec(QCursor::pos());
    if (action == openAction) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(messageFile->getFilePath()));
    } else if (action == openDirAction) {
#if defined(PLATFORM_WINDOWS)
        QDir dir(messageFile->getFilePath());
        QString path = dir.path();
        path.replace("/", "\\");
        qDebug() << path;
        QProcess process;
        QProcess::startDetached("explorer", QStringList() << QString("/select,") << QString("%1").arg(path));
        process.waitForFinished();
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX)
        qDebug() << messageFile->getFilePath();
        QDir dir(messageFile->getFilePath());
        QProcess process;
        QProcess::startDetached("nautilus", QStringList() << QString("%1").arg(messageFile->getFilePath()));
        process.waitForFinished();
#elif defined(PLATFORM_MACOS)
#endif
    } else if (action == copyText) {
        qDebug() << message->message;
        QClipboard *clip = QApplication::clipboard();
        clip->setText(message->message);
    } else if (action == freeCopyText) {
        qDebug() << message->message;
        CopyableTextDialog dialog(message->getMessage());
        dialog.show();
        dialog.exec();
    } else if (action == deleteAction) {
        LFile *lFile = message->getFile();
        qDebug() << "lFile is NULL:" << (lFile == nullptr ? "true" : "false");
        qDebug("lFile：%llX", lFile);
        if (lFile != nullptr) {
            lFile->setNextStep(false);
        }
        deleteTime(message->getUuid());
        MessageDB_V3 &messageDb = MessageDB_V3::instance();
        messageDb.deleteMessage(message->getUuid());
        ui->chatListWidget->removeItemWidget(item);
        delete item;
    }
    delete popMenu;
    delete openAction;
    delete deleteAction;
}

void LANShareWindow::on_chatListWidget_itemDoubleClicked(QListWidgetItem *item) {
    auto *message = (Message *) ui->chatListWidget->itemWidget(item);
    if (message == nullptr) {
        return;
    }
    if (typeid(*message) == typeid(MessageTime)) {
        return;
    }
    if (typeid(*message) == typeid(MessageFile)) {
        auto *messageFile = (MessageFile *) message;
        QDesktopServices::openUrl(QUrl::fromLocalFile(messageFile->getFilePath()));
    } else {
        CopyableTextDialog dialog(message->getMessage());
        dialog.show();
        dialog.exec();
    }
}

void LANShareWindow::on_chatListWidget_itemClicked(QListWidgetItem *item) {
    /* Message *message = (Message *) ui->chatListWidget->itemWidget(item);
     if (message != nullptr && typeid(*message) == typeid(MessageFile)) {
         MessageFile *messageFile = (MessageFile *) message;
         QDesktopServices::openUrl(QUrl::fromLocalFile(messageFile->getFilePath()));
     }*/
}

void LANShareWindow::on_setting_triggered() {
    auto *setting = new Setting(this);
    QObject::connect(setting, SIGNAL(sigUpdateSetting()), this, SLOT(updateSetting()));
    setting->show();
}

void LANShareWindow::deleteTime(const QString &uuid) {
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        QListWidgetItem *tempItem = ui->chatListWidget->item(i);
        auto *msg = dynamic_cast<Message *>(ui->chatListWidget->itemWidget(tempItem));
        if (msg != nullptr && typeid(*msg) == typeid(MessageTime)) {
            const auto *messageTime = dynamic_cast<MessageTime *>(msg);
            if (messageTime->getBindId() ==uuid) {
                ui->chatListWidget->removeItemWidget(tempItem);
                delete msg;
                break;
            }
        }
    }
}
void LANShareWindow::on_actionclaerAll_triggered() {
    MessageDB_V3 &messageDb = MessageDB_V3::instance();
    messageDb.deleteAllMessage();
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        QListWidgetItem *item = ui->chatListWidget->item(i);
        auto *message = (Message *) ui->chatListWidget->itemWidget(item);
        if (message != nullptr && typeid(*message) == typeid(MessageFile)) {
            LFile *lFile = message->getFile();
            if (lFile != nullptr) {
                lFile->setNextStep(false);
            }
        }
    }
    ui->chatListWidget->clear();
    lastMessageTime = 0;
}

void LANShareWindow::on_actionclearMessage_triggered() {
    std::list<QListWidgetItem *> itemList;
    MessageDB_V3 &messageDb = MessageDB_V3::instance();
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        QListWidgetItem *item = ui->chatListWidget->item(i);
        auto *message = (Message *) ui->chatListWidget->itemWidget(item);
        if (message != nullptr && typeid(*message) == typeid(Message)) {
            itemList.push_back(item);
            deleteTime(message->getUuid());
            messageDb.deleteMessage(message->getUuid());
            delete message;
        }
    }
    for (auto item: itemList) {
        ui->chatListWidget->removeItemWidget(item);
        delete item;
    }
}

void LANShareWindow::on_actionclearFile_triggered() {
    std::list<QListWidgetItem *> itemList;
    MessageDB_V3 &messageDb = MessageDB_V3::instance();
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        QListWidgetItem *item = ui->chatListWidget->item(i);
        auto *message = (Message *) ui->chatListWidget->itemWidget(item);
        if (message != nullptr && typeid(*message) == typeid(MessageFile)) {
            itemList.push_back(item);
            deleteTime(message->getUuid());
            messageDb.deleteMessage(message->getUuid());
            LFile *lFile = message->getFile();
            if (lFile != nullptr) {
                lFile->setNextStep(false);
            }
            delete message;
        }
    }
    for (auto item: itemList) {
        ui->chatListWidget->removeItemWidget(item);
        delete item;
    }
}

void LANShareWindow::on_actionabout_triggered() {
    auto *about = new About(this);
    about->show();
}

void LANShareWindow::sendMessage(const bool isClip) {
    QString msg = ui->textEdit->toPlainText();
    if (msg.isEmpty()) {
        return;
    }
    ui->textEdit->setText("");
    auto *messageW = new Message(msg, config.clientName, Device::L_WIN, false, ui->chatListWidget->parentWidget());
    messageW->setUuid(Utils::getUUID());
    checkAndAddChatTime(messageW->getUuid());
    auto *item = new QListWidgetItem(ui->chatListWidget);
    dealMessage(messageW, item);
    ui->chatListWidget->scrollToBottom();
    LANShare::getInstance()->broadcastMessage(currentDevice.getDevName().isEmpty() ? nullptr : &currentDevice, msg,
                                              isClip, true);
    ui->textEdit->setFocus();
    MessageDB_V3 &messageDb = MessageDB_V3::instance();
    messageDb.addMessage(messageW);
}

void LANShareWindow::on_sendButton_clicked() {
    sendMessage();
}

void LANShareWindow::on_select_send_devices_clicked() {
    auto *deviceSelecter = new DeviceSelecter([this](const Device &device) {
        if (device.getDevIp().isEmpty()) {
            currentDevice = Device();
            ui->select_send_devices->setText("所有设备");
        } else {
            currentDevice = device;
            ui->select_send_devices->setText(device.getDevName());
            qDebug() << "选择设备:" << currentDevice.getDevName();
        }
    }, true, LANShare::getInstance(), this);
    deviceSelecter->show();
}

void LANShareWindow::on_select_files_clicked() {
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

void LANShareWindow::updateWebServiceIp() const {
    std::vector<Device> mDevices = LANShare::getInstance()->getMDevices();
    if (config.webService && !mDevices.empty()) {
        ui->web_address->setText("http://" + mDevices[0].getDevIp() + ":" + QString::number(config.tcpPort));
        ui->web_address->show();
    } else {
        ui->web_address->setText("");
        ui->web_address->hide();
    }
}

void LANShareWindow::on_webService_clicked() {
    config.webService = ui->webService->isChecked();
    QSettings *settings = config.getSettings();
    settings->setValue(WEB_SERVICE, config.webService);
    updateWebServiceIp();
}

void LANShareWindow::on_select_directorys_clicked() {
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
    auto *messageW = new Message(
        message, device.getDevName(), device.getDevMode(),
        isLeft, ui->chatListWidget
    );
    messageW->setUuid(Utils::getUUID());
    checkAndAddChatTime(messageW->getUuid());
    auto *item = new QListWidgetItem(ui->chatListWidget);
    dealMessage(messageW, item);
    MessageDB_V3 &messageDb = MessageDB_V3::instance();
    messageDb.addMessage(messageW);
    ui->chatListWidget->scrollToBottom();
}

void LANShareWindow::newWebClient(const QString &token, const QString &ip, const QString &name) {
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
        tokenDBUtil.setPass(token, 1);
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

void LANShareWindow::dealMessage(Message *messageW, QListWidgetItem *item) const {
    item->setSizeHint(messageW->sizeHint());
    ui->chatListWidget->addItem(item);
    ui->chatListWidget->setItemWidget(item, messageW);
}

void LANShareWindow::insertMessage(Message *messageW, QListWidgetItem *item) const {
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
        auto *pMessageFile = (MessageFile *) ui->chatListWidget->itemWidget(ui->chatListWidget->item(i));
        if (pMessageFile != nullptr) {
            pMessageFile->setProgress(0);
        }
    }
    ui->chatListWidget->update();
}

void LANShareWindow::recviceFile(LFile *lFile, const QString &uuid, QString message, QString userName, mlong fileSize,
                                 bool left, bool isFile, bool completed) {
    auto *messageW = new MessageFile(message, userName, 2, left, isFile, ui->chatListWidget->parentWidget());
    messageW->setUuid(uuid);
    messageW->setFileSize(fileSize);
    messageW->setFileSizeStr(QString::fromStdString(Utils::computeSize(fileSize)));
    messageW->setFile(lFile);
    if (completed) {
        messageW->setCompleted(true);
        messageW->setRecviced(true);
    }
    checkAndAddChatTime(messageW->getUuid());
    qDebug() << messageW->getFileSizeStr();
    auto *item = new QListWidgetItem(ui->chatListWidget);
    dealMessage(messageW, item);
    MessageDB_V3 &messageDb = MessageDB_V3::instance();
    messageDb.addMessage(messageW);
    ui->chatListWidget->scrollToBottom();
}

void LANShareWindow::recviceFileProgress(const QString &uuid, int progress) {
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        auto *message = (Message *) ui->chatListWidget->itemWidget(ui->chatListWidget->item(i));
        if (message != nullptr && typeid(*message) == typeid(MessageFile)) {
            auto *messageFile = (MessageFile *) message;
            if (messageFile->getUuid() == uuid) {
                messageFile->setProgress(progress);
                ui->chatListWidget->update();
                break;
            }
        }
    }
}

void LANShareWindow::recviceFileSuccess(const QString &uuid, bool completed, const QString &filePath) {
    MessageDB_V3 &messageDb = MessageDB_V3::instance();
    for (int i = 0; i < ui->chatListWidget->count(); i++) {
        auto *message = (Message *) ui->chatListWidget->itemWidget(ui->chatListWidget->item(i));
        if (message != nullptr && typeid(*message) == typeid(MessageFile)) {
            auto *messageFile = (MessageFile *) message;
            if (messageFile->getUuid() == uuid) {
                messageFile->setRecviced(true);
                messageFile->setFilePath(filePath);
                messageFile->setCompleted(completed);
                messageFile->setFile(nullptr);
                messageDb.updateMessage(messageFile);
                ui->chatListWidget->update();
                break;
            }
        }
    }
}

void LANShareWindow::updateSetting() {
    qDebug() << "updateSetting";
    LANShare::getInstance()->updateMDevices();
    mUtils::setFileAssociation(config.contextMenu);
    ui->webService->setChecked(config.webService);
    if (config.allowBackgroundRunning) {
        createTrayIcon();
    } else {
        closeTrayIcon();
    }
}

void LANShareWindow::loadDeviceList() {
    qDebug() << "点击";
}

void LANShareWindow::closeEvent(QCloseEvent *event) {
    if (config.allowBackgroundRunning) {
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

void LANShareWindow::requstRecvFiles(AcceptFiles *acceptFiles) {
    QMessageBox msgBox(this);
    msgBox.setText("是否接收设备：【" + acceptFiles->device.getDevName() + "】发送的" +
                   QString::number(acceptFiles->files.size()) + "个文件");
    msgBox.setWindowTitle("接收文件请求");
    msgBox.setWindowIcon(QIcon(":/img/ic_launcher.png"));
    msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setWindowFlags(msgBox.windowFlags() | Qt::WindowStaysOnTopHint); // 设置窗口始终显示在顶层
    msgBox.setButtonText(QMessageBox::No, "取消");
    msgBox.setButtonText(QMessageBox::Yes, "确认");
    int ret = msgBox.exec();
    std::thread([ret, acceptFiles]() mutable {
        LANShare::startHandleRecvFile(
            ret == QMessageBox::Yes,
            acceptFiles->device,
            acceptFiles->needEncData,
            acceptFiles->files,
            std::move(acceptFiles->tcpClient)
        );
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

void LANShareWindow::checkVersion() {
    QNetworkRequest request(QUrl(
        QString(LANSHARE_SERVER)
        + "/get_version"
        + "?type=" + APP_TYPE
        + "&versionCode=" + QString::number(LANSHARE_VERSION)
        + "&versionName=" + LANSHARE_VERSION_NAME
    ));
    connect(networkAccessManager, &QNetworkAccessManager::finished, this, &LANShareWindow::checkVersionCallback);
    networkAccessManager->get(request);
}

void LANShareWindow::checkVersionCallback(QNetworkReply *reply) {
    // 检查请求是否成功
    if (reply->error() == QNetworkReply::NoError) {
        // 读取响应数据
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        QJsonObject object = doc.object();
        int versionCode = object["versionCode"].toInt();
        bool isForceUpdate = object["isForceUpdate"].toBool();
        int forceUpdateMinVersion = object["forceUpdateMinVersion"].toInt();
        QString versionName = object["version"].toString();
        QString fileUrl = object["fileUrl"].toString();
        QString updateContent = object["updateContent"].toString();
        QSettings *settings = config.getSettings();
        int forceVersion = settings->value(FORCE_VERSION, -1).toInt();
        isForceUpdate = isForceUpdate && LANSHARE_VERSION < forceUpdateMinVersion;
        if (versionCode > LANSHARE_VERSION
            && versionCode > forceVersion
            || isForceUpdate) {
            QMessageBox msgBox(this);
            msgBox.setText(updateContent);
            msgBox.setWindowTitle("有新版本更新: " + versionName);
            msgBox.setWindowIcon(QIcon(":/img/ic_launcher.png"));
            msgBox.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
            msgBox.setDefaultButton(QMessageBox::No);
            msgBox.setButtonText(QMessageBox::No, isForceUpdate ? "退出" : "不更新");
            msgBox.setButtonText(QMessageBox::Yes, "打开下载链接");
            // 显示消息框，并获取用户的选择
            int ret = msgBox.exec();
            // 根据用户的选择进行相应的操作
            if (ret == QMessageBox::Yes) {
                if (!fileUrl.startsWith("http://")) {
                    fileUrl = LANSHARE_SERVER + fileUrl;
                }
                QDesktopServices::openUrl(fileUrl);
            } else {
                if (isForceUpdate) {
                    QApplication::quit();
                    return;
                }
                settings->setValue(FORCE_VERSION, versionCode);
            }
        }
    } else {
        // 请求失败，输出错误信息
        qDebug() << "Error:" << reply->errorString();
    }
    // 释放资源
    reply->deleteLater();
}

void LANShareWindow::checkAndAddChatTime(const QString &bindId) {
    if (TimeTools::isMoreThanFiveMinutesAgo(lastMessageTime)) {
        lastMessageTime = TimeTools::getCurrentTimestamp();
        auto *messageTime = new MessageTime(lastMessageTime, bindId, ui->chatListWidget->parentWidget());
        messageTime->setUuid(Utils::getUUID());
        auto *itemTime = new QListWidgetItem(ui->chatListWidget);
        dealMessage(messageTime, itemTime);
        MessageDB_V3 &messageDB = MessageDB_V3::instance();
        messageDB.addMessage(messageTime);
    }
}

void LANShareWindow::loadData() {
    if (!loadingData) {
        loadingData = true;
        MessageDB_V3 &messageDb = MessageDB_V3::instance();
        std::list<Message *> data = messageDb.queryList(pageSize, pageCount++, ui->chatListWidget);
        qDebug() << "data:" << data.size();
        qDebug() << "pageCount:" << pageCount;
        qDebug() << "pageSize:" << pageSize;
        for (auto rit = data.rbegin(); rit != data.rend(); ++rit) {
            insertMessage(*rit, new QListWidgetItem());
        }
        // 恢复之前的顶部行号
        ui->chatListWidget->scrollToItem(ui->chatListWidget->item(static_cast<int>(data.size() - 1)),
                                         QAbstractItemView::PositionAtTop);
        loadingData = false;
    }
}
