/*
  settings.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2019 Erik Johansson <erik@ejohansson.se>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

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

signals:
    void prettifySymbolsChanged(bool);
    void collapseTemplatesChanged(bool);
    void collapseDepthChanged(int);
    void colorSchemeChanged(ColorScheme);
    void pathsChanged();

public slots:
    void setPrettifySymbols(bool prettifySymbols);
    void setCollapseTemplates(bool collapseTemplates);
    void setCollapseDepth(int depth);
    void setColorScheme(ColorScheme scheme);
    void setPaths(const QStringList& userPaths, const QStringList& systemPaths);

private:
    Settings() = default;
    ~Settings() = default;

    bool m_prettifySymbols = true;
    bool m_collapseTemplates = true;
    int m_collapseDepth = 1;
    ColorScheme m_colorScheme = ColorScheme::Default;
    QStringList m_userPaths;
    QStringList m_systemPaths;
};
