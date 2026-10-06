//
// Created by user on 2024/5/31.
//

#include "EditTextEventFilter.h"

// 静态单例指针
EditTextEventFilter *EditTextEventFilter::editTextEventFilterInstance;

EditTextEventFilter *EditTextEventFilter::getInstance() {
    if (!editTextEventFilterInstance) {
        editTextEventFilterInstance = new EditTextEventFilter();
    }
    return editTextEventFilterInstance;
}

bool EditTextEventFilter::eventFilter(QObject *obj, QEvent *event) {
    auto *textEdit = dynamic_cast<QTextEdit *>(obj);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            QMenu contextMenu((QWidget *) obj);
            QAction *copyAction = contextMenu.addAction("复制");
            QAction *pasteAction = contextMenu.addAction("粘贴");
            QAction *cutAction = contextMenu.addAction("剪切");
            QAction *selectAllAction = contextMenu.addAction("选择全部");
            contextMenu.addAction("取消");
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QAction *selectedAction = contextMenu.exec(mouseEvent->globalPosition().toPoint());
#else
            QAction *selectedAction = contextMenu.exec(mouseEvent->globalPos());
#endif
            if (selectedAction == copyAction) {
                textEdit->copy();
            } else if (selectedAction == pasteAction) {
                textEdit->paste();
            } else if (selectedAction == cutAction) {
                textEdit->cut();
            } else if (selectedAction == selectAllAction) {
                textEdit->selectAll();
            }
            return true;
        }
    } else if (event->type() == QEvent::ToolTip) {
        return true;
    }
    return QObject::eventFilter(obj, event);
}

EditTextEventFilter::~EditTextEventFilter() {
    delete editTextEventFilterInstance;
    editTextEventFilterInstance = nullptr;
}
