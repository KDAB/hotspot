/*
    SPDX-FileCopyrightText: Erik Johansson <erik@ejohansson.se>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QColor>
#include <QObject>

class Settings : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Settings)

public:
    enum class ColorScheme : int
    {
        Default,
        Binary,
        Kernel,
        System,
        NumColorSchemes
    };
    Q_ENUM(ColorScheme);

    enum class CostAggregation : int
    {
        BySymbol,
        ByThread,
        ByProcess,
        ByCPU
    };
    Q_ENUM(CostAggregation);

    static Settings* instance();

    bool prettifySymbols() const
    {
        return m_prettifySymbols;
    }

    bool collapseTemplates() const
    {
        return m_collapseTemplates;
    }

    int collapseDepth() const
    {
        return m_collapseDepth;
    }

    ColorScheme colorScheme() const
    {
        return m_colorScheme;
    }

    QStringList userPaths() const
    {
        return m_userPaths;
    }

    QStringList systemPaths() const
    {
        return m_systemPaths;
    }

    QStringList debuginfodUrls() const
    {
        return m_debuginfodUrls;
    }

    QString sysroot() const
    {
        return m_sysroot;
    }

    QString kallsyms() const
    {
        return m_kallsyms;
    }

    QString debugPaths() const
    {
        return m_debugPaths;
    }

    QString extraLibPaths() const
    {
        return m_extraLibPaths;
    }

    QString appPath() const
    {
        return m_appPath;
    }

    QString arch() const
    {
        return m_arch;
    }

    QString objdump() const
    {
        return m_objdump;
    }

    int callgraphParentDepth() const
    {
        return m_callgraphParentDepth;
    }

    int callgraphChildDepth() const
    {
        return m_callgraphChildDepth;
    }

    QColor callgraphActiveColor() const
    {
        return m_callgraphActiveColor;
    }

    QColor callgraphColor() const
    {
        return m_callgraphColor;
    }

    CostAggregation costAggregation() const
    {
        return m_costAggregation;
    }

    QString lastUsedEnvironment() const
    {
        return m_lastUsedEnvironment;
    }

    void loadFromFile();

signals:
    void prettifySymbolsChanged(bool);
    void collapseTemplatesChanged(bool);
    void collapseDepthChanged(int);
    void colorSchemeChanged(ColorScheme);
    void costAggregationChanged(CostAggregation);
    void pathsChanged();
    void debuginfodUrlsChanged();
    void sysrootChanged(const QString& path);
    void kallsymsChanged(const QString& path);
    void debugPathsChanged(const QString& paths);
    void extraLibPathsChanged(const QString& paths);
    void appPathChanged(const QString& path);
    void archChanged(const QString& arch);
    void objdumpChanged(const QString& objdump);
    void callgraphChanged();
    void lastUsedEnvironmentChanged(const QString& envName);

public slots:
    void setPrettifySymbols(bool prettifySymbols);
    void setCollapseTemplates(bool collapseTemplates);
    void setCollapseDepth(int depth);
    void setColorScheme(ColorScheme scheme);
    void setPaths(const QStringList& userPaths, const QStringList& systemPaths);
    void setDebuginfodUrls(const QStringList& urls);
    void setSysroot(const QString& path);
    void setKallsyms(const QString& path);
    void setDebugPaths(const QString& paths);
    void setExtraLibPaths(const QString& paths);
    void setAppPath(const QString& path);
    void setArch(const QString& arch);
    void setObjdump(const QString& objdump);
    void setCallgraphParentDepth(int parent);
    void setCallgraphChildDepth(int child);
    void setCallgraphColors(const QColor& active, const QColor& inactive);
    void setCostAggregation(CostAggregation costAggregation);
    void setLastUsedEnvironment(const QString& envName);

private:
    Settings() = default;
    ~Settings() = default;

    bool m_prettifySymbols = true;
    bool m_collapseTemplates = true;
    int m_collapseDepth = 1;
    ColorScheme m_colorScheme = ColorScheme::Default;
    CostAggregation m_costAggregation = CostAggregation::BySymbol;
    QStringList m_userPaths;
    QStringList m_systemPaths;
    QStringList m_debuginfodUrls;

    QString m_sysroot;
    QString m_kallsyms;
    QString m_debugPaths;
    QString m_extraLibPaths;
    QString m_appPath;
    QString m_arch;
    QString m_objdump;

    QString m_lastUsedEnvironment;

    int m_callgraphParentDepth = 3;
    int m_callgraphChildDepth = 2;
    QColor m_callgraphActiveColor;
    QColor m_callgraphColor;
};
