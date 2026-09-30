#include "Setting.h"
#include "ui_Setting.h"
#include "LANShareWindow.h"
#include "Config.h"
#include <QFileDialog>
#include <QDebug>
#include <QMainWindow>

Setting::Setting(QWidget *parent) :
        QWidget(parent),
        ui(new Ui::Setting) {
    ui->setupUi(this);
    setWindowTitle("设置");
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
    setFixedSize(452, 351);
    setWindowModality(Qt::ApplicationModal);
    themeRadio = new QButtonGroup(this);
    themeRadio->addButton(ui->radioButton_auto);
    themeRadio->addButton(ui->radioButton_dark);
    themeRadio->addButton(ui->radioButton_light);
    // 连接QButtonGroup的按钮点击信号到槽函数
    connect(themeRadio, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, &Setting::onThemeRadioButtonClicked);
    ui->webService->setChecked(config.webService);
    ui->openWebService->setChecked(config.openWebService);
    ui->userName->setPlainText(config.clientName);
    ui->filePath->setPlainText(config.saveFilePath);
    ui->tcpPort->setPlainText(QString::number(config.tcpPort));
    ui->udpPort->setPlainText(QString::number(config.udpPort));
    ui->receivceMute->setChecked(config.receivceMute);
    ui->encdata->setChecked(config.encData);
    ui->contextMenu->setChecked(config.contextMenu);
    ui->acceptRecvFiles->setChecked(config.acceptRecvFiles);
    ui->allowBackgroundRunning->setChecked(config.allowBackgroundRunning);
    ui->messageKey->setPlainText(config.messageKey);
    if (config.themeName.isEmpty()) {
        ui->radioButton_auto->setChecked(true);
    } else if (config.themeName == "dark") {
        ui->radioButton_dark->setChecked(true);
    } else if (config.themeName == "light") {
        ui->radioButton_light->setChecked(true);
    }

}

void Setting::onThemeRadioButtonClicked() {
    QAbstractButton *button = themeRadio->checkedButton();
    if (button == ui->radioButton_dark) {
        config.themeName = "dark";
    } else if (button == ui->radioButton_light) {
        config.themeName = "light";
    } else {
        config.themeName = "";
    }
    config.setTheme(config.themeName);
    qDebug() << "themeName:" << config.themeName;
    QSettings *settings = config.getSettings();
    settings->setValue(THEME, config.themeName);
}

Setting::~Setting() {
    delete ui;
}

// 选择文件夹
void Setting::on_selectPath_clicked() {
    QString srcDirPath = QFileDialog::getExistingDirectory(
            this, "选择文件夹",
            config.saveFilePath);
    qDebug() << srcDirPath;
    if (!srcDirPath.isEmpty()) {
        ui->filePath->setPlainText(srcDirPath);
    }
}

void Setting::on_receivceMute_stateChanged(int state) {
    qDebug() << "state:" << state;
}

void Setting::closeEvent(QCloseEvent *event) {
    QSettings *settings = config.getSettings();

    config.clientName = ui->userName->toPlainText();
    config.saveFilePath = ui->filePath->toPlainText();
    config.messageKey = ui->messageKey->toPlainText();
    config.tcpPort = QString(ui->tcpPort->toPlainText()).toInt();
    config.udpPort = QString(ui->udpPort->toPlainText()).toInt();
    config.receivceMute = ui->receivceMute->isChecked();
    config.encData = ui->encdata->isChecked();
    config.contextMenu = ui->contextMenu->isChecked();
    config.webService = ui->webService->isChecked();
    config.openWebService = ui->openWebService->isChecked();
    config.acceptRecvFiles = ui->acceptRecvFiles->isChecked();
    config.allowBackgroundRunning = ui->allowBackgroundRunning->isChecked();

    settings->setValue(USER_NAME, config.clientName);
    settings->setValue(FILE_PATH, config.saveFilePath);
    settings->setValue(TCP_PORT, config.tcpPort);
    settings->setValue(UDP_PORT, config.udpPort);
    settings->setValue(RECEIVE_MUTE, config.receivceMute);
    settings->setValue(ENC_DATA, config.encData);
    settings->setValue(CONTEXT_MENU, config.contextMenu);
    settings->setValue(WEB_SERVICE, config.webService);
    settings->setValue(OPEN_WEB_SERVICE, config.openWebService);
    settings->setValue(ACCEPT_RECV_FILES, config.acceptRecvFiles);
    settings->setValue(ALLOW_BACKGROUND_RUNNING, config.allowBackgroundRunning);

    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::ECB, QAESEncryption::PKCS7);
    QByteArray input(config.messageKey.toUtf8());
    QByteArray key = QString(KEY).toUtf8();
    QByteArray encodedText = encryption.encode(input, key).toHex();
    settings->setValue(MESSAGE_KEY, QString(encodedText));
    emit sigUpdateSetting();
}

