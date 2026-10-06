#include "include/CaseInsensitiveMap.h"

// 实现比较函数
bool CaseInsensitiveMap::CaseInsensitiveComparator::operator()(const QString &lhs, const QString &rhs) const {
    return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
}

// 实现插入方法
void CaseInsensitiveMap::insert(const QString &key, const QString &value) {
    map[key] = value;
}

// 实现获取值方法
QString CaseInsensitiveMap::value(const QString &key, const QString &defaultValue) const {
    auto it = map.find(key);
    if (it != map.end()) {
        return it->second;
    }
    return defaultValue;
}

// 实现检查是否包含键方法
bool CaseInsensitiveMap::contains(const QString &key) const {
    return map.find(key) != map.end();
}

// 实现删除键值对方法
void CaseInsensitiveMap::remove(const QString &key) {
    map.erase(key);
}

// 实现获取所有键方法
QList<QString> CaseInsensitiveMap::keys() const {
    QList<QString> result;
    for (const auto &pair : map) {
        result.append(pair.first);
    }
    return result;
}

// 实现检查是否为空方法
bool CaseInsensitiveMap::isEmpty() const {
    return map.empty();
}

// 实现清空所有元素方法
void CaseInsensitiveMap::clear() {
    map.clear();
}

// 实现获取元素个数方法
int CaseInsensitiveMap::size() const {
    return map.size();
}
