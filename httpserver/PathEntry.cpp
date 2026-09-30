#include "PathEntry.h"

PathEntry::PathEntry(const QString &path, HttpHandler httpHandler) : path(path), httpHandler(httpHandler) {}

PathEntry::PathEntry(const QString &path, const QString &method, HttpHandler httpHandler)
        : path(path), httpHandler(httpHandler), method(method.toUpper()) {
}

const QString &PathEntry::getMethod() const {
    return method;
}

void PathEntry::setMethod(const QString &method) {
    PathEntry::method = method.toUpper();
}

QString PathEntry::getPath() const {
    return path;
}

void PathEntry::setPath(const QString &path) {
    this->path = path;
}

HttpHandler PathEntry::getHttpHandler() const {
    return httpHandler;
}

void PathEntry::setHttpHandler(HttpHandler httpHandler) {
    this->httpHandler = httpHandler;
}

