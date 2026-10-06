#include "DeviceSelecter.h"
#include "ui_DeviceSelecter.h"
#include "LANShare.h"
#include "DeviceItem.h"
#include "DeviceSelecterThread.h"
#include <utility>

DeviceSelecter::DeviceSelecter(std::function<void(Device)> callback, bool showAllItem, LANShare *lanShare,
                               QWidget *parent) : QWidget(parent),
                                                  ui(new Ui::DeviceSelecter),
                                                  callback(std::move(callback)),
                                                  showAllItem(showAllItem),
                                                  lanShare(lanShare) {
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
    setFixedSize(270, 330);
    setWindowModality(Qt::ApplicationModal);
    setWindowTitle(tr("选择设备"));
    ui->selecterListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->selecterListWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(sigUpdateList()),
            this, SLOT(slotUpdateListUI()));
    this->lanShare = lanShare;
    slotUpdateListUI();
    selecterThread = new DeviceSelecterThread(this, lanShare);
    selecterThread->start();
}

DeviceSelecter::~DeviceSelecter() {
    delete ui;
    delete selecterThread;
}

void DeviceSelecter::on_selecterListWidget_itemClicked(QListWidgetItem *item) {
    auto *deviceItem = (DeviceItem *) ui->selecterListWidget->itemWidget(item);
    callback(deviceItem->getDevice());
    QWidget::close();
}

void DeviceSelecter::on_selecterListWidget_customContextMenuRequested(const QPoint &pos) {
    QListWidgetItem *item = ui->selecterListWidget->itemAt(pos);
    if (item == nullptr)
        return;
    auto *deviceItem = reinterpret_cast<DeviceItem *>(ui->selecterListWidget->itemWidget(item));
    if (deviceItem == nullptr || deviceItem->getDevice().getDevMode() == Device::L_UNKNOW) {
        return;
    }
    Device device = deviceItem->getDevice();
    auto *popMenu = new QMenu(this);
    auto *copyIP = new QAction(tr("复制IP"), this);
    auto *copyName = new QAction(tr("复制名称"), this);
    auto *copyUrl = new QAction(tr("复制网页地址"), this);
    auto *copyAll = new QAction(tr("复制所有"), this);
    popMenu->addAction(copyIP);
    popMenu->addAction(copyName);
    popMenu->addAction(copyUrl);
    popMenu->addAction(copyAll);
    QAction *action = popMenu->exec(QCursor::pos());
    QClipboard *clip = QApplication::clipboard();
    if (action == copyIP) {
        clip->setText(device.getDevIp());
        QWidget::close();
    } else if (action == copyName) {
        clip->setText(device.getDevName());
        QWidget::close();
    } else if (action == copyUrl) {
        clip->setText("http://" + device.getDevIp() + ":" + QString::number(device.getDevPort()));
        QWidget::close();
    } else if (action == copyAll) {
        QString info = tr("设备名称: ") + device.getDevName() + "\n";
        info += tr("设备电量: ") + QString::number(std::max(0, device.getBatteryLevel())) + "%\n";
        info += tr("是否在充电: ") + QString(device.getChargeStatus() == 1 ? tr("是") : tr("否")) + "\n";
        info += (tr("设备IP: ") + device.getDevIp() + "\n");
        info += (tr("网页地址: http://") + device.getDevIp() + ":" + QString::number(device.getDevPort()));
        clip->setText(info);
        QWidget::close();
    }
    delete popMenu;
    delete copyIP;
    delete copyName;
    delete copyUrl;
    delete copyAll;
}


int DeviceSelecter::getDeviceCount() const {
    return deviceCount;
}

void DeviceSelecter::slotUpdateListUI() {
    ui->selecterListWidget->clear();
    if (showAllItem) {
        auto *item = new QListWidgetItem;
        auto *pDeviceItem = new DeviceItem(ui->selecterListWidget);
        Device device;
        device.setDevName(tr("所有设备"));
        device.setDevIp("");
        device.setDevMode(Device::L_UNKNOW);
        pDeviceItem->setDevice(device);
        item->setSizeHint(QSize(width() - 25, 50));
        ui->selecterListWidget->addItem(item);
        ui->selecterListWidget->setItemWidget(item, pDeviceItem);
    }
    std::map<std::string, Device> onLineDevices = lanShare->getOnLineDevices();
    std::map<std::string, Device>::iterator iter;
    for (iter = onLineDevices.begin(); iter != onLineDevices.end(); iter++) {
        auto *item = new QListWidgetItem;
        auto *pDeviceItem = new DeviceItem(ui->selecterListWidget);
        pDeviceItem->setDevice(iter->second);
        item->setSizeHint(QSize(width() - 25, 50));
        ui->selecterListWidget->addItem(item);
        ui->selecterListWidget->setItemWidget(item, pDeviceItem);
    }
    ui->selecterListWidget->update();
    deviceCount = (int) onLineDevices.size();
}

void DeviceSelecter::closeEvent(QCloseEvent *event) {
    close = true;
}

bool DeviceSelecter::isClose() const {
    return close;
}
