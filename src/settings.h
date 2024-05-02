/*
    SPDX-FileCopyrightText: Erik Johansson <erik@ejohansson.se>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

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
        CostRatio,
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

    QString perfMapPath() const
    {
        return m_perfMapPath;
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

    QString sourceCodePaths() const
    {
        return m_sourceCodePaths;
    }

    QString perfPath() const
    {
        return m_perfPath;
    }

    bool showBranches() const
    {
        return m_showBranches;
    }
    bool showHexdump() const
    {
        return m_showHexdump;
    }

    int tabWidth() const
    {
        return m_tabWidth;
    }

    static constexpr int DefaultTabWidth = 4;

    void loadFromFile();

signals:
    void prettifySymbolsChanged(bool);
    void collapseTemplatesChanged(bool);
    void collapseDepthChanged(int);
    void colorSchemeChanged(Settings::ColorScheme);
    void costAggregationChanged(Settings::CostAggregation);
    void pathsChanged();
    void debuginfodUrlsChanged();
    void sysrootChanged(const QString& path);
    void kallsymsChanged(const QString& path);
    void debugPathsChanged(const QString& paths);
    void extraLibPathsChanged(const QString& paths);
    void appPathChanged(const QString& path);
    void archChanged(const QString& arch);
    void objdumpChanged(const QString& objdump);
    void perfMapPathChanged(const QString& perfMapPath);
    void callgraphChanged();
    void lastUsedEnvironmentChanged(const QString& envName);
    void sourceCodePathsChanged(const QString& paths);
    void perfPathChanged(const QString& perfPath);
    void showBranchesChanged(bool showBranches);
    void showHexdumpChanged(bool showHexdump);
    void tabWidthChanged(int distance);

public slots:
    void setPrettifySymbols(bool prettifySymbols);
    void setCollapseTemplates(bool collapseTemplates);
    void setCollapseDepth(int depth);
    void setColorScheme(Settings::ColorScheme scheme);
    void setPaths(const QStringList& userPaths, const QStringList& systemPaths);
    void setDebuginfodUrls(const QStringList& urls);
    void setSysroot(const QString& path);
    void setKallsyms(const QString& path);
    void setDebugPaths(const QString& paths);
    void setExtraLibPaths(const QString& paths);
    void setAppPath(const QString& path);
    void setArch(const QString& arch);
    void setObjdump(const QString& objdump);
    void setPerfMapPath(const QString& perfMapPath);
    void setCallgraphParentDepth(int parent);
    void setCallgraphChildDepth(int child);
    void setCallgraphColors(const QColor& active, const QColor& inactive);
    void setCostAggregation(Settings::CostAggregation costAggregation);
    void setLastUsedEnvironment(const QString& envName);
    void setSourceCodePaths(const QString& paths);
    void setPerfPath(const QString& path);
    void setShowBranches(bool showBranches);
    void setShowHexdump(bool showHexdump);
    void setTabWidth(int distance);

private:
    using QObject::QObject;
    ~Settings();

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
    QString m_sourceCodePaths;
    QString m_perfMapPath;
    bool m_showBranches = true;
    bool m_showHexdump = false;
    int m_tabWidth = DefaultTabWidth;

    QString m_lastUsedEnvironment;

    int m_callgraphParentDepth = 3;
    int m_callgraphChildDepth = 2;
    QColor m_callgraphActiveColor;
    QColor m_callgraphColor;

    QString m_perfPath;
};
