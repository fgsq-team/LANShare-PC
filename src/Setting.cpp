#include "Setting.h"
#include "ui_Setting.h"
#include "LANShareWindow.h"
#include "Config.hpp"
#include "TranslationManager.h"
#include "LHttpServer.h"
#include <QFileDialog>
#include <QDebug>
#include <QMainWindow>
#include <QSettings>
#include <QCoreApplication>

Setting::Setting(QWidget *parent) : QWidget(parent),
                                    ui(new Ui::Setting) {
    ui->setupUi(this);
    setWindowTitle(tr("设置"));
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
    setFixedSize(500, 380);
    setWindowModality(Qt::ApplicationModal);
    themeRadio = new QButtonGroup(this);
    themeRadio->addButton(ui->radioButton_auto);
    themeRadio->addButton(ui->radioButton_dark);
    themeRadio->addButton(ui->radioButton_light);
    themeRadio->addButton(ui->radioButton_emerald);
    // 连接QButtonGroup的按钮点击信号到槽函数
    connect(themeRadio, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, &Setting::onThemeRadioButtonClicked);
    ui->webService->setChecked(Config::instance().webService);
    ui->openWebService->setChecked(Config::instance().openWebService);
    ui->userName->setPlainText(Config::instance().clientName);
    ui->filePath->setPlainText(Config::instance().saveFilePath);
    ui->tcpPort->setPlainText(QString::number(Config::instance().tcpPort));
    ui->udpPort->setPlainText(QString::number(Config::instance().udpPort));
    ui->receivceMute->setChecked(Config::instance().receivceMute);
    ui->encdata->setChecked(Config::instance().encData);
    ui->contextMenu->setChecked(Config::instance().contextMenu);
    ui->acceptRecvFiles->setChecked(Config::instance().acceptRecvFiles);
    ui->allowBackgroundRunning->setChecked(Config::instance().allowBackgroundRunning);
    ui->autoStart->setChecked(Config::instance().autoStart);
    ui->messageKey->setPlainText(Config::instance().messageKey);
    
    // 设置语言选择框
    if (Config::instance().language == "zh_CN") {
        ui->languageComboBox->setCurrentIndex(0);
    } else if (Config::instance().language == "en") {
        ui->languageComboBox->setCurrentIndex(1);
    }
    
    if (Config::instance().themeName.isEmpty()) {
        ui->radioButton_auto->setChecked(true);
    } else if (Config::instance().themeName == "dark") {
        ui->radioButton_dark->setChecked(true);
    } else if (Config::instance().themeName == "light") {
        ui->radioButton_light->setChecked(true);
    } else if (Config::instance().themeName == "emerald") {
        ui->radioButton_emerald->setChecked(true);
    }
}

/**
 * 主题切换槽函数
 * 根据选中的单选按钮更新主题配置，并通知所有网页客户端切换主题
 */
void Setting::onThemeRadioButtonClicked() {
    QAbstractButton *button = themeRadio->checkedButton();
    if (button == ui->radioButton_dark) {
        Config::instance().themeName = "dark";
    } else if (button == ui->radioButton_light) {
        Config::instance().themeName = "light";
    } else if (button == ui->radioButton_emerald) {
        Config::instance().themeName = "emerald";
    } else {
        Config::instance().themeName = "";
    }
    Config::instance().setTheme(Config::instance().themeName);
    qDebug() << "themeName:" << Config::instance().themeName;
    QSettings *settings = Config::instance().getSettings();
    settings->setValue(THEME, Config::instance().themeName);
    // 通知所有已连接的网页客户端切换主题
    LHttpServer::sendTheme();
}

Setting::~Setting() {
    delete ui;
}

// 选择文件夹
void Setting::on_selectPath_clicked() {
    QString srcDirPath = QFileDialog::getExistingDirectory(
        this, tr("选择文件夹"),
        Config::instance().saveFilePath);
    qDebug() << srcDirPath;
    if (!srcDirPath.isEmpty()) {
        ui->filePath->setPlainText(srcDirPath);
    }
}

void Setting::on_receivceMute_stateChanged(int state) {
    qDebug() << "state:" << state;
}

void Setting::closeEvent(QCloseEvent *event) {
    QSettings *settings = Config::instance().getSettings();

    Config::instance().clientName = ui->userName->toPlainText();
    Config::instance().saveFilePath = QDir::cleanPath(ui->filePath->toPlainText());
    Config::instance().messageKey = ui->messageKey->toPlainText();
    Config::instance().tcpPort = QString(ui->tcpPort->toPlainText()).toInt();
    Config::instance().udpPort = QString(ui->udpPort->toPlainText()).toInt();
    Config::instance().receivceMute = ui->receivceMute->isChecked();
    Config::instance().encData = ui->encdata->isChecked();
    Config::instance().contextMenu = ui->contextMenu->isChecked();
    Config::instance().webService = ui->webService->isChecked();
    Config::instance().openWebService = ui->openWebService->isChecked();
    Config::instance().acceptRecvFiles = ui->acceptRecvFiles->isChecked();
    Config::instance().allowBackgroundRunning = ui->allowBackgroundRunning->isChecked();
    Config::instance().autoStart = ui->autoStart->isChecked();
    
    // 保存语言设置
    if (ui->languageComboBox->currentIndex() == 0) {
        Config::instance().language = "zh_CN";
    } else if (ui->languageComboBox->currentIndex() == 1) {
        Config::instance().language = "en";
    }
    
    // 应用语言设置
    TranslationManager::instance()->setLanguage(Config::instance().language);

    settings->setValue(USER_NAME, Config::instance().clientName);
    settings->setValue(FILE_PATH, Config::instance().saveFilePath);
    settings->setValue(TCP_PORT, Config::instance().tcpPort);
    settings->setValue(UDP_PORT, Config::instance().udpPort);
    settings->setValue(RECEIVE_MUTE, Config::instance().receivceMute);
    settings->setValue(ENC_DATA, Config::instance().encData);
    settings->setValue(CONTEXT_MENU, Config::instance().contextMenu);
    settings->setValue(WEB_SERVICE, Config::instance().webService);
    settings->setValue(OPEN_WEB_SERVICE, Config::instance().openWebService);
    settings->setValue(ACCEPT_RECV_FILES, Config::instance().acceptRecvFiles);
    settings->setValue(ALLOW_BACKGROUND_RUNNING, Config::instance().allowBackgroundRunning);
    settings->setValue(AUTO_START, Config::instance().autoStart);
    settings->setValue("language", Config::instance().language);

#ifdef Q_OS_WIN
    setAutoStart(Config::instance().autoStart);
#endif
    QByteArray key = QString(KEY).toUtf8();
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::ECB, QAESEncryption::PKCS7);
    QByteArray input(Config::instance().messageKey.toUtf8());
    QByteArray encodedText = encryption.encode(input, key).toHex();
    settings->setValue(MESSAGE_KEY, QString(encodedText));
    emit sigUpdateSetting();
}

#ifdef Q_OS_WIN
void Setting::setAutoStart(bool enable) {
    QSettings reg("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                  QSettings::NativeFormat);
    QString appName = APP_NAME;
    if (enable) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        reg.setValue(appName, appPath);
    } else {
        reg.remove(appName);
    }
}
#endif
