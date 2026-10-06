
#ifndef MESSAGEFILE_H
#define MESSAGEFILE_H

#include <QWidget>
#include <QTextEdit>
#include <QProgressBar>
#include "Message.h"

class QPaintEvent;

class QPainter;

class QLabel;

class QMovie;


class MessageFile : public Message {
Q_OBJECT
public:
    MessageFile(QString message, QString userName, int devMode, bool left, bool isFile, QWidget *parent = nullptr);

    int getProgress() const;

    void setProgress(int progress);

    const QString &getFilePath() const;

    void setFilePath(const QString &filePath);

    mlong getFileSize() const;

    void setFileSize(mlong fileSize);

    bool isRecviced() const;

    void setRecviced(bool recviced);

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    int progress = 0;
    mlong fileSize = 0;
    bool recviced = false;
    bool completed = false;
    bool m_isFile = true;

    QString filePath;
    QString fileSizeStr;
    QLabel *m_avatarLabel;
    QLabel *m_nicknameLabel;
    QLabel *m_fileIconLabel;
    QLabel *m_fileSizeLabel;
    QLabel *m_statusLabel;
    QVBoxLayout *textLayout;
    QHBoxLayout *mainHLayout;
    QHBoxLayout *mainLayout;

    QSize docSize{};
public:
    const QString &getFileSizeStr() const;

    void setFileSizeStr(const QString &fileSizeStr);

    bool isCompleted() const;

    void setCompleted(bool completed);

    bool isFile() const;

    void setIsFile(bool isFile);

private:
    void initView();

    void adjustTextWidth();

    void updateFileIcon();

    QPixmap loadSvgPixmap(const QString &path, int size) const;

};

#endif // MESSAGEFILE_H
