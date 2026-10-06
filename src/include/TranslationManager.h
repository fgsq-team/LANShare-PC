#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QObject>
#include <QTranslator>
#include <QApplication>
#include <QSettings>
#include "Config.hpp"

class TranslationManager : public QObject
{
    Q_OBJECT

public:
    static TranslationManager* instance();
    void setLanguage(const QString& language);
    QString getCurrentLanguage() const;
    void loadLanguageFromSettings();

private:
    explicit TranslationManager(QObject *parent = nullptr);
    static TranslationManager* m_instance;
    QTranslator* m_translator;
    QString m_currentLanguage;
};

#endif // TRANSLATIONMANAGER_H