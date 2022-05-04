/*
    SPDX-FileCopyrightText: Erik Johansson <erik@ejohansson.se>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QThread>

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
