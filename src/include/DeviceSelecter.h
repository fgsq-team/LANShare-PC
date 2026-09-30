#ifndef DEVICESELECTER_H
#define DEVICESELECTER_H

#include <QWidget>
#include "LANShare.h"
#include "DeviceSelecterThread.h"

namespace Ui {
    class DeviceSelecter;
}

class DeviceSelecter : public QWidget {
Q_OBJECT

public:
    explicit DeviceSelecter(std::function<void(Device)> callback,bool showAllItem, LANShare *lanShare = nullptr, QWidget *parent = nullptr);

    ~DeviceSelecter();

    int getDeviceCount() const;

    bool isClose() const;

signals:

    void sigUpdateList();

public slots:

    void slotUpdateListUI();

    void on_selecterListWidget_itemClicked(QListWidgetItem *item);

    void on_selecterListWidget_customContextMenuRequested(const QPoint &pos);

protected :
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::DeviceSelecter *ui;
    LANShare *lanShare;
    int deviceCount = 0;
    bool close = false;
    bool showAllItem = false;
    DeviceSelecterThread *selecterThread;
    std::function<void(Device)> callback;
};

#endif // DEVICESELECTER_H
