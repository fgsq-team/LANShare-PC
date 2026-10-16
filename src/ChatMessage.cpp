//
// ChatMessage 数据模型的便捷实现。
//
#include "ChatMessage.h"
#include "LFile.h"
#include <QFileInfo>

ChatMessage ChatMessage::fromText(const QString &text, const QString &deviceName,
                                  int devMode, ChatDirection direction) {
    ChatMessage msg;
    msg.deviceName = deviceName;
    msg.devMode = devMode;
    msg.direction = direction;

    ContentElement el;
    el.type = ElementType::Text;
    el.text = text;
    msg.elements.push_back(el);
    return msg;
}

ChatMessage ChatMessage::fromFile(const QString &name, const QString &elementUuid,
                                  const QString &deviceName, int devMode,
                                  ChatDirection direction, mlong size, bool isDirectory) {
    ChatMessage msg;
    msg.deviceName = deviceName;
    msg.devMode = devMode;
    msg.direction = direction;

    ContentElement el;
    el.type = ElementType::File;
    el.name = name;
    el.text = name;
    el.elementUuid = elementUuid;
    el.size = size;
    el.isDirectory = isDirectory;
    msg.elements.push_back(el);
    return msg;
}

ChatMessage ChatMessage::fromImage(const QString &path, const QString &name,
                                   const QString &elementUuid, const QString &deviceName,
                                   int devMode, ChatDirection direction, mlong size) {
    ChatMessage msg;
    msg.deviceName = deviceName;
    msg.devMode = devMode;
    msg.direction = direction;

    ContentElement el;
    el.type = ElementType::Image;
    el.name = name;
    el.text = name;
    el.filePath = path;
    el.elementUuid = elementUuid;
    el.size = size;
    msg.elements.push_back(el);
    return msg;
}

bool ChatMessage::isImageName(const QString &name) {
    static const QStringList imageExts = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "ico", "tif", "tiff", "svg"
    };
    return imageExts.contains(QFileInfo(name).suffix().toLower());
}

bool ChatMessage::hasMedia() const {
    for (const auto &el: elements) {
        if (el.type == ElementType::File || el.type == ElementType::Image) {
            return true;
        }
    }
    return false;
}

const ContentElement *ChatMessage::firstOf(ElementType type) const {
    for (const auto &el: elements) {
        if (el.type == type) {
            return &el;
        }
    }
    return nullptr;
}

ContentElement *ChatMessage::firstOf(ElementType type) {
    for (auto &el: elements) {
        if (el.type == type) {
            return &el;
        }
    }
    return nullptr;
}

ContentElement *ChatMessage::findElement(const QString &elementUuid) {
    if (elementUuid.isEmpty()) {
        return nullptr;
    }
    for (auto &el: elements) {
        if (el.elementUuid == elementUuid) {
            return &el;
        }
    }
    return nullptr;
}

QString ChatMessage::displayText() const {
    const ContentElement *text = firstOf(ElementType::Text);
    if (text != nullptr) {
        return text->text;
    }
    if (!elements.isEmpty()) {
        const auto &el = elements.first();
        if (!el.name.isEmpty()) {
            return el.name;
        }
        return el.text;
    }
    return QString();
}

QVector<LFile *> ChatMessage::collectFiles() const {
    QVector<LFile *> files;
    for (const auto &el: elements) {
        if (el.file != nullptr) {
            files.push_back(el.file);
        }
    }
    return files;
}
