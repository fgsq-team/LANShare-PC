#ifndef SETTING_H
#define SETTING_H

#include <QWidget>
#include <QButtonGroup>

namespace Ui {
    class Setting;
}

class Setting : public QWidget {
Q_OBJECT

private:
    void closeEvent(QCloseEvent *event);
#ifdef Q_OS_WIN
    void setAutoStart(bool enable);
#endif

public:
    explicit Setting(QWidget *parent = nullptr);

    ~Setting();

public slots:

    void on_selectPath_clicked();
    void on_receivceMute_stateChanged(int state);
    void onThemeRadioButtonClicked();

signals:

    void sigUpdateSetting();

private:
    Ui::Setting *ui;
    QButtonGroup *themeRadio;

};

#endif // SETTING_H
