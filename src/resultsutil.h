/*
  resultsutil.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

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

#pragma once

#include <QtGlobal>
#include <QTreeView>
#include <QHeaderView>
#include <KFilterProxySearchLine>
#include <KRecursiveFilterProxyModel>
#include "callercalleemodel.h"
#include "costdelegate.h"

namespace ResultsUtil {
static void stretchFirstColumn(QTreeView* view)
{
    view->header()->setStretchLastSection(false);
    view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
}

template<typename Model>
void setupTreeView(QTreeView* view, KFilterProxySearchLine* filter, Model* model)
{
    auto proxy = new KRecursiveFilterProxyModel(view);
    proxy->setSortRole(Model::SortRole);
    proxy->setFilterRole(Model::FilterRole);
    proxy->setSourceModel(model);

    filter->setProxy(proxy);

    view->sortByColumn(Model::InitialSortColumn);
    view->setModel(proxy);
    stretchFirstColumn(view);

    view->setContextMenuPolicy(Qt::CustomContextMenu);
}

template<typename Model>
void setupCostDelegate(Model* model, QTreeView* view)
{
    auto costDelegate = new CostDelegate(Model::SortRole, Model::TotalCostRole, view);
    QObject::connect(model, &QAbstractItemModel::modelReset,
                     costDelegate, [costDelegate, model, view]() {
                        for (int i = Model::NUM_BASE_COLUMNS, c = model->columnCount(); i < c; ++i) {
                            view->setItemDelegateForColumn(i, costDelegate);
                        }
                    });
}

static void hideEmptyColumns(const Data::Costs& costs, QTreeView* view, int numBaseColumns)
{
    for (int i = 0; i < costs.numTypes(); ++i) {
        if (!costs.totalCost(i)) {
            view->hideColumn(numBaseColumns + i);
        }
    }
}
}
