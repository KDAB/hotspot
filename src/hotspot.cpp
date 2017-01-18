/*
  hotspot.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

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

#include "hotspot.h"

#include "parsers/perf/perfparser.h"
#include "models/costmodel.h"

#include <QTreeView>
#include <QSortFilterProxyModel>

Hotspot::Hotspot(QObject* parent)
    : QObject(parent)
    , m_model(new CostModel(this))
    , m_parser(new PerfParser(this))
{
    // TODO: move into separate mainwindow class
    auto view = new QTreeView;
    view->setSortingEnabled(true);

    m_model = new CostModel(view);

    auto proxy = new QSortFilterProxyModel(view);
    proxy->setSourceModel(m_model);

    view->sortByColumn(CostModel::SelfCost);
    view->setModel(proxy);
    view->show();

    connect(m_parser, &PerfParser::bottomUpDataAvailable,
            this, [this] (const FrameData& data) {
                m_model->setData(data);
            });

    // TODO: show results when finished
    // TODO: show error messages
}

Hotspot::~Hotspot() = default;

void Hotspot::openFile(const QString& path)
{
    // TODO: support input files of different types via plugins
    m_parser->startParseFile(path);
}
