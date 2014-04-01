#include "configmapper.h"
#include "config_builder.h"
#include "services/config.h"
#include "services/pluginmanager.h"
#include "customconfigwidgetplugin.h"
#include "common/colorbutton.h"
#include "common/fontedit.h"
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QGroupBox>
#include <QDebug>

#define APPLY_CFG(Widget, Value, WidgetType, Method, DataType, Notifier) \
    if (qobject_cast<WidgetType*>(Widget))\
    {\
        qobject_cast<WidgetType*>(Widget)->Method(Value.value<DataType>());\
        connect(Widget, Notifier, this, SIGNAL(modified()));\
        return;\
    }

#define APPLY_CFG2(Widget, Value, WidgetType, Method, DataType, Notifier, ExtraConditionMethod) \
    if (qobject_cast<WidgetType*>(Widget) && qobject_cast<WidgetType*>(Widget)->ExtraConditionMethod())\
    {\
        qobject_cast<WidgetType*>(Widget)->Method(Value.value<DataType>());\
        connect(Widget, Notifier, this, SIGNAL(modified()));\
        return;\
    }

#define SAVE_CFG(Widget, Key, WidgetType, Method) \
    if (qobject_cast<WidgetType*>(Widget))\
    {\
        Key->set(qobject_cast<WidgetType*>(Widget)->Method());\
        return;\
    }

#define SAVE_CFG2(Widget, Key, WidgetType, Method, ExtraConditionMethod) \
    if (qobject_cast<WidgetType*>(Widget) && qobject_cast<WidgetType*>(Widget)->ExtraConditionMethod())\
    {\
        Key->set(qobject_cast<WidgetType*>(Widget)->Method());\
        return;\
    }

ConfigMapper::ConfigMapper(CfgMain* cfgMain)
{
    this->cfgMainList << cfgMain;
}

ConfigMapper::ConfigMapper(const QList<CfgMain*> cfgMain) :
    cfgMainList(cfgMain)
{
}

void ConfigMapper::loadToWidget(QWidget *widget)
{
    QHash<QString, CfgEntry *> allConfigEntries = getAllConfigEntries();
    QList<QWidget*> allConfigWidgets = getAllConfigWidgets(widget);
    QHash<QString,QVariant> config;

    if (isPersistant())
        config = CFG->getAll();

    foreach (QWidget* widget, allConfigWidgets)
        applyConfigToWidget(widget, allConfigEntries, config);
}

void ConfigMapper::loadToWidget(CfgEntry* config, QWidget* widget)
{
    QVariant configValue = config->get();
    if (applyCustomConfigToWidget(config, widget, configValue))
        return;

    applyCommonConfigToWidget(widget, configValue);
}

void ConfigMapper::saveFromWidget(QWidget *widget)
{
    QHash<QString, CfgEntry *> allConfigEntries = getAllConfigEntries();
    QList<QWidget*> allConfigWidgets = getAllConfigWidgets(widget);

    if (isPersistant())
        CFG->beginMassSave();

    foreach (QWidget* w, allConfigWidgets)
        saveWidget(w, allConfigEntries);

    if (isPersistant())
        CFG->commitMassSave();
}


void ConfigMapper::applyConfigToWidget(QWidget* widget, const QHash<QString, CfgEntry *> &allConfigEntries, const QHash<QString,QVariant>& config)
{
    CfgEntry* cfgEntry = getConfigEntry(widget, allConfigEntries);
    if (!cfgEntry)
        return;

    QVariant configValue;
    if (config.contains(cfgEntry->getFullKey()))
    {
        configValue = config[cfgEntry->getFullKey()];
        if (!configValue.isValid())
            configValue = cfgEntry->getDefultValue();
    }
    else
    {
        configValue = cfgEntry->getDefultValue();
    }

    if (applyCustomConfigToWidget(cfgEntry, widget, configValue))
        return;

    applyCommonConfigToWidget(widget, configValue);
}

bool ConfigMapper::applyCustomConfigToWidget(CfgEntry* key, QWidget* widget, const QVariant& value)
{
    CustomConfigWidgetPlugin* handler;
    QList<CustomConfigWidgetPlugin*> handlers;
    handlers += internalCustomConfigWidgets;
    handlers += PLUGINS->getLoadedPlugins<CustomConfigWidgetPlugin>();

    foreach (handler, handlers)
    {
        if (handler->isConfigForWidget(key, widget))
        {
            handler->applyConfigToWidget(key, widget, value);
            connect(widget, handler->getModifiedNotifier(), this, SIGNAL(modified()));
            return true;
        }
    }
    return false;
}

void ConfigMapper::saveWidget(QWidget* widget, const QHash<QString, CfgEntry *> &allConfigEntries)
{
    CfgEntry* cfgEntry = getConfigEntry(widget, allConfigEntries);
    if (!cfgEntry)
        return;

    if (saveCustomConfigFromWidget(widget, cfgEntry))
        return;

    saveCommonConfigFromWidget(widget, cfgEntry);
}

void ConfigMapper::applyCommonConfigToWidget(QWidget *widget, const QVariant &value)
{
    APPLY_CFG(widget, value, QCheckBox, setChecked, bool, SIGNAL(stateChanged(int)));
    APPLY_CFG(widget, value, QLineEdit, setText, QString, SIGNAL(textChanged(QString)));
    APPLY_CFG(widget, value, QSpinBox, setValue, int, SIGNAL(valueChanged(QString)));
    APPLY_CFG(widget, value, QComboBox, setCurrentText, QString, SIGNAL(currentTextChanged(QString)));
    APPLY_CFG(widget, value, FontEdit, setFont, QFont, SIGNAL(fontChanged(QFont)));
    APPLY_CFG(widget, value, ColorButton, setColor, QColor, SIGNAL(colorChanged(QColor)));
    APPLY_CFG2(widget, value, QGroupBox, setChecked, bool, SIGNAL(clicked(bool)), isCheckable);

    qWarning() << "Unhandled config widget type (for APPLY_CFG):" << widget->metaObject()->className()
               << "with value:" << value;
}

void ConfigMapper::saveCommonConfigFromWidget(QWidget* widget, CfgEntry* key)
{
    SAVE_CFG(widget, key, QCheckBox, isChecked);
    SAVE_CFG(widget, key, QLineEdit, text);
    SAVE_CFG(widget, key, QSpinBox, value);
    SAVE_CFG(widget, key, QComboBox, currentText);
    SAVE_CFG(widget, key, FontEdit, getFont);
    SAVE_CFG(widget, key, ColorButton, getColor);
    SAVE_CFG2(widget, key, QGroupBox, isChecked, isCheckable);

    qWarning() << "Unhandled config widget type (for SAVE_CFG):" << widget->metaObject()->className();
}

bool ConfigMapper::saveCustomConfigFromWidget(QWidget* widget, CfgEntry* key)
{
    CustomConfigWidgetPlugin* plugin;
    QList<CustomConfigWidgetPlugin*> handlers;
    handlers += internalCustomConfigWidgets;
    handlers += PLUGINS->getLoadedPlugins<CustomConfigWidgetPlugin>();

    foreach (plugin, handlers)
    {
        if (plugin->isConfigForWidget(key, widget))
        {
            plugin->saveWidgetToConfig(widget, key);
            return true;
        }
    }
    return false;
}

CfgEntry* ConfigMapper::getConfigEntry(QWidget* widget, const QHash<QString, CfgEntry*>& allConfigEntries)
{
    QString key = widget->statusTip();
    if (!allConfigEntries.contains(key))
    {
        qCritical() << "Config entries don't contain key" << key
                    << "but it was requested by ConfigMapper::getConfigEntry().";
        return nullptr;
    }

    return allConfigEntries[key];
}

QHash<QString, CfgEntry *> ConfigMapper::getAllConfigEntries()
{
    QHash<QString, CfgEntry*> entries;
    QString key;
    for (CfgMain* cfgMain : cfgMainList)
    {
        QHashIterator<QString,CfgCategory*> catIt(cfgMain->getCategories());
        while (catIt.hasNext())
        {
            catIt.next();
            QHashIterator<QString,CfgEntry*> entryIt( catIt.value()->getEntries());
            while (entryIt.hasNext())
            {
                entryIt.next();
                key = catIt.key()+"."+entryIt.key();
                if (entries.contains(key))
                {
                    qCritical() << "Duplicate config entry key:" << key;
                    continue;
                }
                entries[key] = entryIt.value();
            }
        }
    }
    return entries;
}

QList<QWidget*> ConfigMapper::getAllConfigWidgets(QWidget *parent)
{
    QList<QWidget*> results;
    QWidget* widget = nullptr;
    foreach (QObject* obj, parent->children())
    {
        widget = qobject_cast<QWidget*>(obj);
        if (!widget)
            continue;

        results += getAllConfigWidgets(widget);
        if (widget->statusTip().isEmpty())
            continue;

        results << widget;
    }
    return results;
}

bool ConfigMapper::isPersistant() const
{
    for (CfgMain* cfgMain : cfgMainList)
    {
        if (cfgMain->isPersistable())
            return true;
    }
    return false;
}

void ConfigMapper::setInternalCustomConfigWidgets(const QList<CustomConfigWidgetPlugin*>& value)
{
    internalCustomConfigWidgets = value;
}

