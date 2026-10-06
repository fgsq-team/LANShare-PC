#include "CopyableTextDialog.h"
#include "EditTextEventFilter.h"
#include <QApplication>
#include <QMessageBox>
#include <QClipboard>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>

CopyableTextDialog::CopyableTextDialog(const QString &initialText, QWidget *parent) : QDialog(parent) {
    setupUI();
    textEdit->setPlainText(initialText);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowModality(Qt::ApplicationModal);
    setMinimumSize(200, 200);
    setWindowIcon(QIcon(":/img/copy.png"));
}

void CopyableTextDialog::copyText() {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(textEdit->toPlainText());
    QMessageBox::information(this, tr("提示"), tr("文本已复制到剪切板"));
    close();
}

void CopyableTextDialog::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    textEdit = new QTextEdit(this);
    textEdit->installEventFilter(EditTextEventFilter::getInstance());

    layout->addWidget(textEdit);
    QPushButton *copyButton = new QPushButton(tr("复制全部文本"), this);
    connect(copyButton, &QPushButton::clicked, this, &CopyableTextDialog::copyText);
    layout->addWidget(copyButton);
    setLayout(layout);
    setWindowTitle(tr("自由复制文本"));
}