
#ifndef MESSAGE_H
#define MESSAGE_H

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QMenu>
#include <QMouseEvent>
#include <QWidget>
#include <QClipboard>
#include "LFile.h"

#define MESSAGE_TYPE 1
#define MESSAGE_FILE_TYPE 2
#define MESSAGE_TIME_TYPE 3

class QPaintEvent;

class QPainter;

class QLabel;

class QMovie;

class CustomTextBrowser : public QTextBrowser {
Q_OBJECT
private:
    bool flag = false;
public:
    explicit CustomTextBrowser(QWidget *parent = nullptr) : QTextBrowser(parent) {
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
//        setContextMenuPolicy(Qt::NoContextMenu);
        installEventFilter(this);
    }

    QSize sizeHint() const override {
        QSize size = QTextBrowser::sizeHint();
        size.setHeight(document()->size().height());
        return size;
    }

    QMargins mViewportMargins() const {
        return QTextBrowser::viewportMargins();
    }

    void clearSelection() {
        QTextCursor cursor = textCursor();
        cursor.clearSelection();
        setTextCursor(cursor);
    }

    void menu(const QPoint &pos) {
        flag = true;
        QMenu contextMenu(this);
        QAction *copyAction = contextMenu.addAction("复制");
        QAction *selectAllAction = contextMenu.addAction("选择全部");
        QAction *selectedAction = contextMenu.exec(pos);
        if (selectedAction == copyAction) {
            QTextCursor cursor = textCursor();
            if (cursor.hasSelection()) {
                copy();
            } else {
                QString textToCopy = toPlainText();
                QClipboard *clipboard = QApplication::clipboard();
                clipboard->setText(textToCopy);
            }
        } else if (selectedAction == selectAllAction) {
            selectAll();
        }
        flag = false;
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        auto *textEdit = dynamic_cast<QTextEdit *>(obj);
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                menu(mouseEvent->globalPosition().toPoint());
#else
                menu(mouseEvent->globalPos());
#endif
                return true;
            }
        } else if (event->type() == QEvent::ToolTip) {
            return true;
        }
        return QObject::eventFilter(obj, event);
    }

    void focusOutEvent(QFocusEvent *event) override {
        if (!flag) {
            clearSelection();
        }
        QTextBrowser::focusOutEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override {

    }

};

class Message : public QWidget {
Q_OBJECT
public:
    Message(QWidget *parent = nullptr);

    Message(QString message, QString userName, int devMode, bool left, QWidget *parent = nullptr);

    inline QString text() { return message; }

    const QString &getDeviceName() const;

    bool isLeft() const;

    const QString &getUuid() const;

    void setUuid(const QString &uuid);

    LFile *getFile() const;

    void setFile(LFile *file);

    int getDevMode() const;

    void setDevMode(int devMode);

public:
    QSize sizeHint() const override;

    void setStyleSheet(const QString &sheet);

protected:
    void paintEvent(QPaintEvent *event);

    void resizeEvent(QResizeEvent *event) override;

    void changeEvent(QEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override {
        // 忽略鼠标移动事件，避免改变颜色
        QWidget::mouseMoveEvent(event);
        event->ignore();
    }

    void leaveEvent(QEvent *event) override {
        // 忽略离开事件，避免改变颜色
        QWidget::leaveEvent(event);
        event->ignore();
    }

private:
    void initView();

public:
    QString message;
    QString userName;
    LFile *file = nullptr;
    bool left = false;
    int dataVersion = 0;
    int devMode = -1;
    QString uuid;
    QLabel *m_avatarLabel = nullptr;
    QLabel *m_nicknameLabel = nullptr;
    CustomTextBrowser *m_textLabel = nullptr;

    const QString &getMessage() const;

    const QString &getUserName() const;

    int getDataVersion() const;

    void setDataVersion(int dataVersion);

    void adjustTextWidth();
};

#endif // MESSAGE_H
