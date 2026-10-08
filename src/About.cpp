#include "About.h"
#include "ui_about.h"
#include "Config.hpp"
#include "UpdateChecker.h"

About::About(QWidget *parent) : QWidget(parent),
                                ui(new Ui::About) {
    ui->setupUi(this);
    setWindowTitle("关于");
    setFixedSize(384, 350);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setWindowModality(Qt::ApplicationModal);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ui->versionName->setText(QString("V") + LANSHARE_VERSION_NAME);
#else
    ui->versionName->setText(QString("V") + LANSHARE_VERSION_NAME + "_win7");
#endif

    // 设置二维码标签的大小
    // ui->alipayQRCode->setFixedSize(100, 100);
    // ui->wechatQRCode->setFixedSize(100, 100);

    // 设置捐赠标签样式
    ui->donateLabel->setStyleSheet("QLabel { color : #FF5722; font-weight: bold; }");

    // 检查更新：手动触发一次检测
    connect(ui->labelCheckUpdate, &QLabel::linkActivated, this, [this](const QString &) {
        UpdateChecker::check(this, true);
    });
}

About::~About() {
    delete ui;
}