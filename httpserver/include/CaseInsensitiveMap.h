#ifndef CASEINSENSITIVEMAP_H
#define CASEINSENSITIVEMAP_H

#include <map>
#include <QString>
#include <QList>

class CaseInsensitiveMap
{
private:
    struct CaseInsensitiveComparator {
        bool operator()(const QString &lhs, const QString &rhs) const;
    };

    std::map<QString, QString, CaseInsensitiveComparator> map;

public:
    // 构造函数
    CaseInsensitiveMap() = default;
    
    // 插入键值对
    void insert(const QString &key, const QString &value);
    
    // 获取值
    QString value(const QString &key, const QString &defaultValue = QString()) const;
    
    // 检查是否包含键
    bool contains(const QString &key) const;
    
    // 删除键值对
    void remove(const QString &key);
    
    // 获取所有键（按忽略大小写排序）
    QList<QString> keys() const;
    
    // 检查是否为空
    bool isEmpty() const;
    
    // 清空所有元素
    void clear();
    
    // 获取元素个数
    int size() const;
};

#endif // CASEINSENSITIVEMAP_H
