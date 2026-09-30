#include "About.h"
#include "ui_about.h"
#include "Config.h"

About::About(QWidget *parent) :
        QWidget(parent),
        ui(new Ui::About) {
    ui->setupUi(this);
    setWindowTitle("关于");
    setFixedSize(384, 245);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setWindowModality(Qt::ApplicationModal);
    ui->versionName->setText(QString("V") + LANSHARE_VERSION_NAME);
}

About::~About() {
    delete ui;
}
