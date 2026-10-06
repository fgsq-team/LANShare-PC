#include "TranslationManager.h"
#include <QDebug>

TranslationManager* TranslationManager::m_instance = nullptr;

TranslationManager::TranslationManager(QObject *parent)
    : QObject(parent)
    , m_translator(new QTranslator(this))
    , m_currentLanguage("zh_CN")
{
}

TranslationManager* TranslationManager::instance()
{
    if (!m_instance) {
        m_instance = new TranslationManager();
    }
    return m_instance;
}

void TranslationManager::setLanguage(const QString& language)
{
    // 如果语言没有改变，则直接返回
    if (m_currentLanguage == language) {
        return;
    }

    // 移除当前翻译器
    qApp->removeTranslator(m_translator);

    // 加载新语言
    if (language != "zh_CN") { // 中文不需要翻译器
        QString qmPath = ":/translations/translations/lanshare_" + language + ".qm";
        if (m_translator->load(qmPath)) {
            qApp->installTranslator(m_translator);
        } else {
            qDebug() << "Failed to load translation file:" << qmPath;
        }
    }

    m_currentLanguage = language;
    
    // 保存语言设置到配置文件
    Config::instance().language = language;
    Config::instance().save();
}

QString TranslationManager::getCurrentLanguage() const
{
    return m_currentLanguage;
}

void TranslationManager::loadLanguageFromSettings()
{
    setLanguage(Config::instance().language);
}