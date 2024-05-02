/*
    SPDX-FileCopyrightText: Erik Johansson <erik@ejohansson.se>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QDir>
#include <QPalette>
#include <QThread>

#include <KColorScheme>
#include <KConfigGroup>
#include <KSharedConfig>

#include "settings.h"

Settings::~Settings() = default;

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

void Settings::setPerfMapPath(const QString& perfMapPath)
{
    m_perfMapPath = perfMapPath;
    emit perfMapPathChanged(m_perfMapPath);
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
        emit costAggregationChanged(costAggregation);
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

    auto config = sharedConfig->group(QStringLiteral("Settings"));
    setPrettifySymbols(config.readEntry("prettifySymbols", true));
    setCollapseTemplates(config.readEntry("collapseTemplates", true));
    setCollapseDepth(config.readEntry("collapseDepth", 1));

    connect(Settings::instance(), &Settings::prettifySymbolsChanged, this, [sharedConfig](bool prettifySymbols) {
        sharedConfig->group(QStringLiteral("Settings")).writeEntry("prettifySymbols", prettifySymbols);
    });

    connect(Settings::instance(), &Settings::collapseTemplatesChanged, this, [sharedConfig](bool collapseTemplates) {
        sharedConfig->group(QStringLiteral("Settings")).writeEntry("collapseTemplates", collapseTemplates);
    });

    connect(this, &Settings::collapseDepthChanged, this, [sharedConfig](int collapseDepth) {
        sharedConfig->group(QStringLiteral("Settings")).writeEntry("collapseDepth", collapseDepth);
    });

    const QStringList userPaths = {QDir::homePath()};
    const QStringList systemPaths = {QDir::rootPath()};
    setPaths(sharedConfig->group(QStringLiteral("PathSettings")).readEntry("userPaths", userPaths),
             sharedConfig->group(QStringLiteral("PathSettings")).readEntry("systemPaths", systemPaths));
    connect(this, &Settings::pathsChanged, this, [sharedConfig, this] {
        sharedConfig->group(QStringLiteral("PathSettings")).writeEntry("userPaths", this->userPaths());
        sharedConfig->group(QStringLiteral("PathSettings")).writeEntry("systemPaths", this->systemPaths());
    });

    // fix build error in app image build
    const auto colorScheme = KColorScheme(QPalette::Normal, KColorScheme::View, sharedConfig);
    const auto color = colorScheme.background(KColorScheme::AlternateBackground).color().name();
    const auto currentColor = colorScheme.background(KColorScheme::ActiveBackground).color().name();

    setCallgraphParentDepth(sharedConfig->group(QStringLiteral("CallgraphSettings")).readEntry("parent", 3));
    setCallgraphChildDepth(sharedConfig->group(QStringLiteral("CallgraphSettings")).readEntry("child", 3));
    setCallgraphColors(sharedConfig->group(QStringLiteral("CallgraphSettings")).readEntry("activeColor", currentColor),
                       sharedConfig->group(QStringLiteral("CallgraphSettings")).readEntry("color", color));
    connect(this, &Settings::callgraphChanged, this, [sharedConfig, this] {
        sharedConfig->group(QStringLiteral("CallgraphSettings")).writeEntry("parent", this->callgraphParentDepth());
        sharedConfig->group(QStringLiteral("CallgraphSettings")).writeEntry("child", this->callgraphChildDepth());
        sharedConfig->group(QStringLiteral("CallgraphSettings"))
            .writeEntry("activeColor", this->callgraphActiveColor());
        sharedConfig->group(QStringLiteral("CallgraphSettings")).writeEntry("color", this->callgraphColor());
    });

    setDebuginfodUrls(sharedConfig->group(QStringLiteral("debuginfod")).readEntry("urls", QStringList()));
    connect(this, &Settings::debuginfodUrlsChanged, this, [sharedConfig, this] {
        sharedConfig->group(QStringLiteral("debuginfod")).writeEntry("urls", this->debuginfodUrls());
    });

    m_lastUsedEnvironment = sharedConfig->group(QStringLiteral("PerfPaths")).readEntry("lastUsed");

    if (!m_lastUsedEnvironment.isEmpty()) {
        auto currentConfig = sharedConfig->group(QStringLiteral("PerfPaths")).group(m_lastUsedEnvironment);
        setSysroot(currentConfig.readEntry("sysroot", ""));
        setAppPath(currentConfig.readEntry("appPath", ""));
        setExtraLibPaths(currentConfig.readEntry("extraLibPaths", ""));
        setDebugPaths(currentConfig.readEntry("debugPaths", ""));
        setKallsyms(currentConfig.readEntry("kallsyms", ""));
        setArch(currentConfig.readEntry("arch", ""));
        setObjdump(currentConfig.readEntry("objdump", ""));
        setPerfMapPath(currentConfig.readEntry("perfMapPath", ""));
    }

    setPerfPath(sharedConfig->group(QStringLiteral("Perf")).readEntry("path", ""));
    connect(this, &Settings::perfPathChanged, this, [sharedConfig](const QString& perfPath) {
        sharedConfig->group(QStringLiteral("Perf")).writeEntry("path", perfPath);
    });

    connect(this, &Settings::lastUsedEnvironmentChanged, this, [sharedConfig](const QString& envName) {
        sharedConfig->group(QStringLiteral("PerfPaths")).writeEntry("lastUsed", envName);
    });

    setSourceCodePaths(sharedConfig->group(QStringLiteral("Disassembly")).readEntry("sourceCodePaths", QString()));
    connect(this, &Settings::sourceCodePathsChanged, this, [sharedConfig](const QString& paths) {
        sharedConfig->group(QStringLiteral("Disassembly")).writeEntry("sourceCodePaths", paths);
    });

    setShowBranches(sharedConfig->group(QStringLiteral("Disassembly")).readEntry("showBranches", true));
    connect(this, &Settings::showBranchesChanged, [sharedConfig](bool showBranches) {
        sharedConfig->group(QStringLiteral("Disassembly")).writeEntry("showBranches", showBranches);
    });

    setShowBranches(sharedConfig->group(QStringLiteral("Disassembly")).readEntry("showHexdump", false));
    connect(this, &Settings::showHexdumpChanged, [sharedConfig](bool showHexdump) {
        sharedConfig->group(QStringLiteral("Disassembly")).writeEntry("showHexdump", showHexdump);
    });

    setTabWidth(sharedConfig->group(QStringLiteral("Disassembly")).readEntry("tabWidth", DefaultTabWidth));
    connect(this, &Settings::tabWidthChanged, [sharedConfig](int distance) {
        sharedConfig->group(QStringLiteral("Disassembly")).writeEntry("tabWidth", distance);
    });
}

void Settings::setSourceCodePaths(const QString& paths)
{
    if (m_sourceCodePaths != paths) {
        m_sourceCodePaths = paths;
        emit sourceCodePathsChanged(m_sourceCodePaths);
    }
}

void Settings::setPerfPath(const QString& path)
{
    if (m_perfPath != path) {
        m_perfPath = path;
        emit perfPathChanged(m_perfPath);
    }
}

void Settings::setShowBranches(bool showBranches)
{
    if (m_showBranches != showBranches) {
        m_showBranches = showBranches;
        emit showBranchesChanged(m_showBranches);
    }
}

void Settings::setShowHexdump(bool showHexdump)
{
    if (m_showHexdump != showHexdump) {
        m_showHexdump = showHexdump;
        emit showHexdumpChanged(m_showHexdump);
    }
}

void Settings::setTabWidth(int distance)
{
    if (m_tabWidth != distance) {
        m_tabWidth = distance;
        emit tabWidthChanged(m_tabWidth);
    }
}
