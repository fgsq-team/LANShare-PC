//
// Created by fgsq on 2024/1/3.
//

#ifndef LANSHARE_COPYABLETEXTDIALOG_H
#define LANSHARE_COPYABLETEXTDIALOG_H

#include <QDialog>

class QTextEdit;
class QPushButton;

class CopyableTextDialog : public QDialog
{
Q_OBJECT

public:
    CopyableTextDialog(const QString &initialText, QWidget *parent = nullptr);

private slots:
    void copyText();

private:
    void setupUI();

    QTextEdit *textEdit;
};


#endif //LANSHARE_COPYABLETEXTDIALOG_H
