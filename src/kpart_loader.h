#ifndef KPART_LOADER_H
#define KPART_LOADER_H

#include <KPluginFactory>

template<typename T>
KPluginFactory::Result<T> instantiatePart(const KPluginMetaData& data, QWidget* parentWidget = nullptr,
                                          QObject* parent = nullptr, const QVariantList& args = {})
{
    KPluginFactory::Result<T> result;
    KPluginFactory::Result<KPluginFactory> factoryResult = KPluginFactory::loadFactory(data);
    if (!factoryResult.plugin) {
        result.errorString = factoryResult.errorString;
        result.errorReason = factoryResult.errorReason;
        return result;
    }
    T* instance = factoryResult.plugin->create<T>(parentWidget, parent, QString(), args);
    if (!instance) {
        result.errorString = QStringLiteral("KPluginFactory could not load the plugin: %1").arg(data.fileName());
        result.errorText = QStringLiteral("KPluginFactory could not load the plugin: %1").arg(data.fileName());
        result.errorReason = KPluginFactory::INVALID_KPLUGINFACTORY_INSTANTIATION;
    } else {
        result.plugin = instance;
    }
    return result;
}

#endif // KPART_LOADER_H
