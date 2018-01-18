/*
  resultsutil.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2018 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "resultsutil.h"

#include <QTreeView>
#include <QHeaderView>
#include <QMenu>
#include <QCoreApplication>

#include <KRecursiveFilterProxyModel>
#include <KFilterProxySearchLine>

#include "models/costdelegate.h"
#include "models/data.h"

namespace ResultsUtil {

void stretchFirstColumn(QTreeView* view)
{
    view->header()->setStretchLastSection(false);
    view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
}

void setupTreeView(QTreeView* view, KFilterProxySearchLine* filter,
                   QAbstractItemModel* model, int initialSortColumn,
                   int sortRole, int filterRole)
{
    auto proxy = new KRecursiveFilterProxyModel(view);
    proxy->setSortRole(sortRole);
    proxy->setFilterRole(filterRole);
    proxy->setSourceModel(model);

    filter->setProxy(proxy);

    view->sortByColumn(initialSortColumn);
    view->setModel(proxy);
    stretchFirstColumn(view);
}

void setupContextMenu(QTreeView* view, int symbolRole,
                      std::function<void(const Data::Symbol&)> callback)
{
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(view, &QTreeView::customContextMenuRequested,
                     view, [view, symbolRole, callback](const QPoint &point) {
                        const auto index = view->indexAt(point);
                        if (!index.isValid()) {
                            return;
                        }

                        QMenu contextMenu;
                        auto *viewCallerCallee = contextMenu.addAction(QCoreApplication::translate("Util", "View Caller/Callee"));
                        auto *action = contextMenu.exec(QCursor::pos());
                        if (action == viewCallerCallee) {
                            const auto symbol = index.data(symbolRole).value<Data::Symbol>();

                            if (symbol.isValid()) {
                                callback(symbol);
                            }
                        }
                    });
}

void setupCostDelegate(QAbstractItemModel* model, QTreeView* view,
                       int sortRole, int totalCostRole, int numBaseColumns)
{
    auto costDelegate = new CostDelegate(sortRole, totalCostRole, view);
    QObject::connect(model, &QAbstractItemModel::modelReset,
                     costDelegate, [costDelegate, model, view, numBaseColumns]() {
                        for (int i = numBaseColumns, c = model->columnCount(); i < c; ++i) {
                            view->setItemDelegateForColumn(i, costDelegate);
                        }
                    });
}

void hideEmptyColumns(const Data::Costs& costs, QTreeView* view, int numBaseColumns)
{
    for (int i = 0; i < costs.numTypes(); ++i) {
        if (!costs.totalCost(i)) {
            view->hideColumn(numBaseColumns + i);
        }
    }
}

}
