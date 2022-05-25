/*
    SPDX-FileCopyrightText: Erik Johansson <erik@ejohansson.se>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QPalette>
#include <QThread>

#include <KColorScheme>
#include <KConfigGroup>
#include <KSharedConfig>

#include "settings.h"

Settings* Settings::instance()
{
    static Settings settings;
    Q_ASSERT(QThread::currentThread() == settings.thread());
    return &settings;
}

void Settings::setPrettifySymbols(bool prettifySymbols)
{
    if (m_prettifySymbols != prettifySymbols) {
        m_prettifySymbols = prettifySymbols;
        emit prettifySymbolsChanged(m_prettifySymbols);
    }
}

void Settings::setCollapseTemplates(bool collapseTemplates)
{
    if (m_collapseTemplates != collapseTemplates) {
        m_collapseTemplates = collapseTemplates;
        emit collapseTemplatesChanged(m_collapseTemplates);
    }
}

void Settings::setCollapseDepth(int depth)
{
    depth = std::max(1, depth);
    if (m_collapseDepth != depth) {
        m_collapseDepth = depth;
        emit collapseDepthChanged(m_collapseDepth);
    }
}

void Settings::setColorScheme(Settings::ColorScheme scheme)
{
    if (m_colorScheme != scheme) {
        m_colorScheme = scheme;
        emit colorSchemeChanged(m_colorScheme);
    }
}

void Settings::setPaths(const QStringList& userPaths, const QStringList& systemPaths)
{
    if (m_userPaths != userPaths || m_systemPaths != systemPaths) {
        m_userPaths = userPaths;
        m_systemPaths = systemPaths;
        emit pathsChanged();
    }
}

void Settings::setDebuginfodUrls(const QStringList& urls)
{
    if (m_debuginfodUrls != urls) {
        m_debuginfodUrls = urls;
        emit debuginfodUrlsChanged();
    }
}

void Settings::setSysroot(const QString& path)
{
    m_sysroot = path.trimmed();
    emit sysrootChanged(m_sysroot);
}

void Settings::setKallsyms(const QString& path)
{
    m_kallsyms = path;
    emit kallsymsChanged(m_kallsyms);
}

void Settings::setDebugPaths(const QString& paths)
{
    m_debugPaths = paths;
    emit debugPathsChanged(m_debugPaths);
}

void Settings::setExtraLibPaths(const QString& paths)
{
    m_extraLibPaths = paths;
    emit extraLibPathsChanged(m_extraLibPaths);
}

void Settings::setAppPath(const QString& path)
{
    m_appPath = path;
    emit appPathChanged(m_appPath);
}

void Settings::setArch(const QString& arch)
{
    m_arch = arch;
    emit archChanged(m_arch);
}

void Settings::setObjdump(const QString& objdump)
{
    m_objdump = objdump;
    emit objdumpChanged(m_objdump);
}

void Settings::setCallgraphParentDepth(int parent)
{
    if (m_callgraphParentDepth != parent) {
        m_callgraphParentDepth = parent;
        emit callgraphChanged();
    }
}

void Settings::setCallgraphChildDepth(int child)
{
    if (m_callgraphChildDepth != child) {
        m_callgraphChildDepth = child;
        emit callgraphChanged();
    }
}

void Settings::setCallgraphColors(const QColor& active, const QColor& inactive)
{
    if (m_callgraphActiveColor != active || m_callgraphColor != inactive) {
        m_callgraphActiveColor = active;
        m_callgraphColor = inactive;
        emit callgraphChanged();
    }
}

void Settings::setCostAggregation(Settings::CostAggregation costAggregation)
{
    if (m_costAggregation != costAggregation) {
        m_costAggregation = costAggregation;
        emit costAggregationChanged(m_costAggregation);
    }
}

void Settings::setLastUsedEnvironment(const QString& envName)
{
    if (envName != m_lastUsedEnvironment) {
        m_lastUsedEnvironment = envName;
        emit lastUsedEnvironmentChanged(m_lastUsedEnvironment);
    }
}

void Settings::loadFromFile()
{
    auto sharedConfig = KSharedConfig::openConfig();

    auto config = sharedConfig->group("Settings");
    setPrettifySymbols(config.readEntry("prettifySymbols", true));
    setCollapseTemplates(config.readEntry("collapseTemplates", true));
    setCollapseDepth(config.readEntry("collapseDepth", 1));

    connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, [sharedConfig](bool prettifySymbols) {
        sharedConfig->group("Settings").writeEntry("prettifySymbols", prettifySymbols);
    });

    connect(Settings::instance(), &Settings::collapseTemplatesChanged, this, [sharedConfig](bool collapseTemplates) {
        sharedConfig->group("Settings").writeEntry("collapseTemplates", collapseTemplates);
    });

    connect(this, &Settings::collapseDepthChanged, this, [sharedConfig](int collapseDepth) {
        sharedConfig->group("Settings").writeEntry("collapseDepth", collapseDepth);
    });

    const QStringList userPaths = {QDir::homePath()};
    const QStringList systemPaths = {QDir::rootPath()};
    setPaths(sharedConfig->group("PathSettings").readEntry("userPaths", userPaths),
             sharedConfig->group("PathSettings").readEntry("systemPaths", systemPaths));
    connect(this, &Settings::pathsChanged, this, [sharedConfig, this] {
        sharedConfig->group("PathSettings").writeEntry("userPaths", this->userPaths());
        sharedConfig->group("PathSettings").writeEntry("systemPaths", this->systemPaths());
    });

    // fix build error in app image build
    const auto colorScheme = KColorScheme(QPalette::Normal, KColorScheme::View, sharedConfig);
    const auto color = colorScheme.background(KColorScheme::AlternateBackground).color().name();
    const auto currentColor = colorScheme.background(KColorScheme::ActiveBackground).color().name();

    setCallgraphParentDepth(sharedConfig->group("CallgraphSettings").readEntry("parent", 3));
    setCallgraphChildDepth(sharedConfig->group("CallgraphSettings").readEntry("child", 3));
    setCallgraphColors(sharedConfig->group("CallgraphSettings").readEntry("activeColor", currentColor),
                       sharedConfig->group("CallgraphSettings").readEntry("color", color));
    connect(this, &Settings::callgraphChanged, this, [sharedConfig, this] {
        sharedConfig->group("CallgraphSettings").writeEntry("parent", this->callgraphParentDepth());
        sharedConfig->group("CallgraphSettings").writeEntry("child", this->callgraphChildDepth());
        sharedConfig->group("CallgraphSettings").writeEntry("activeColor", this->callgraphActiveColor());
        sharedConfig->group("CallgraphSettings").writeEntry("color", this->callgraphColor());
    });

    setDebuginfodUrls(sharedConfig->group("debuginfod").readEntry("urls", QStringList()));
    connect(this, &Settings::debuginfodUrlsChanged, this,
            [sharedConfig, this] { sharedConfig->group("debuginfod").writeEntry("urls", this->debuginfodUrls()); });

    m_lastUsedEnvironment = sharedConfig->group("PerfPaths").readEntry("lastUsed");

    if (!m_lastUsedEnvironment.isEmpty()) {
        auto currentConfig = sharedConfig->group("PerfPaths").group(m_lastUsedEnvironment);
        setSysroot(currentConfig.readEntry("sysroot", ""));
        setAppPath(currentConfig.readEntry("appPath", ""));
        setExtraLibPaths(currentConfig.readEntry("extraLibPaths", ""));
        setDebugPaths(currentConfig.readEntry("debugPaths", ""));
        setKallsyms(currentConfig.readEntry("kallsyms", ""));
        setArch(currentConfig.readEntry("arch", ""));
        setObjdump(currentConfig.readEntry("objdump", ""));
    }
    connect(this, &Settings::lastUsedEnvironmentChanged, [sharedConfig, this](const QString& envName) {
        sharedConfig->group("PerfPaths").writeEntry("lastUsed", envName);
    });
}
