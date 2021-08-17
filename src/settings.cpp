/*
  settings.cpp

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

#include <QDir>

#include "settings.h"

Settings* Settings::instance()
{
    static Settings settings;
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
