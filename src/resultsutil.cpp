/*
  resultsutil.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QComboBox>
#include <QCoreApplication>
#include <QHeaderView>
#include <QMenu>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QLineEdit>

#include <KLocalizedString>

#include "models/costdelegate.h"
#include "models/data.h"
#include "models/filterandzoomstack.h"

#include "costheaderview.h"

namespace ResultsUtil {

void setupHeaderView(QTreeView* view)
{
    view->setHeader(new CostHeaderView(view));
}

void connectFilter(QLineEdit *filter, QSortFilterProxyModel *proxy)
{
    auto *timer = new QTimer(filter);
    timer->setSingleShot(true);

    filter->setClearButtonEnabled(true);
    filter->setPlaceholderText(QCoreApplication::translate("Util", "Search"));

    proxy->setFilterKeyColumn(-1);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    QObject::connect(timer, &QTimer::timeout, proxy, [filter, proxy]() {
        proxy->setFilterFixedString(filter->text());
    });
    QObject::connect(filter, &QLineEdit::textChanged, timer, [timer]() {
        timer->start(300);
    });
}

void setupTreeView(QTreeView* view, QLineEdit* filter, QAbstractItemModel* model, int initialSortColumn,
                   int sortRole, int filterRole)
{
    auto proxy = new QSortFilterProxyModel(view);
    proxy->setRecursiveFilteringEnabled(true);
    proxy->setSortRole(sortRole);
    proxy->setFilterRole(filterRole);
    proxy->setSourceModel(model);
    connectFilter(filter, proxy);

    view->sortByColumn(initialSortColumn, Qt::DescendingOrder);
    view->setModel(proxy);
    setupHeaderView(view);
}

void addFilterActions(QMenu* menu, const Data::Symbol& symbol, FilterAndZoomStack* filterStack)
{
    if (symbol.isValid()) {
        auto filterActions = filterStack->actions();
        filterActions.filterInBySymbol->setData(QVariant::fromValue(symbol));
        filterActions.filterOutBySymbol->setData(filterActions.filterInBySymbol->data());

        menu->addAction(filterActions.filterInBySymbol);
        menu->addAction(filterActions.filterOutBySymbol);
        menu->addSeparator();
    }

    menu->addAction(filterStack->actions().filterOut);
    menu->addAction(filterStack->actions().resetFilter);
}

void setupContextMenu(QTreeView* view, int symbolRole, FilterAndZoomStack* filterStack, CallbackActions actions,
                      std::function<void(CallbackAction action, const Data::Symbol&)> callback)
{
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(view, &QTreeView::customContextMenuRequested, view, [=](const QPoint& point) {
        const auto index = view->indexAt(point);
        const auto symbol = index.data(symbolRole).value<Data::Symbol>();

        QMenu contextMenu;
        if (callback && symbol.isValid() && actions) {
            if (actions.testFlag(CallbackAction::ViewCallerCallee)) {
                auto* viewCallerCallee =
                    contextMenu.addAction(QCoreApplication::translate("Util", "View Caller/Callee"));
                QObject::connect(viewCallerCallee, &QAction::triggered, &contextMenu,
                                 [symbol, callback]() { callback(CallbackAction::ViewCallerCallee, symbol); });
            }
            if (actions.testFlag(CallbackAction::OpenEditor)) {
                auto* openEditorAction = contextMenu.addAction(QCoreApplication::translate("Util", "Open in Editor"));
                QObject::connect(openEditorAction, &QAction::triggered, &contextMenu,
                                 [symbol, callback]() { callback(CallbackAction::OpenEditor, symbol); });
            }
            contextMenu.addSeparator();
        }
        addFilterActions(&contextMenu, symbol, filterStack);

        if (!contextMenu.actions().isEmpty()) {
            contextMenu.exec(QCursor::pos());
        }
    });
}

void setupCostDelegate(QAbstractItemModel* model, QTreeView* view, int sortRole, int totalCostRole, int numBaseColumns)
{
    auto costDelegate = new CostDelegate(sortRole, totalCostRole, view);
    QObject::connect(model, &QAbstractItemModel::modelReset, costDelegate,
                     [costDelegate, model, view, numBaseColumns]() {
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

void fillEventSourceComboBox(QComboBox* combo, const Data::Costs& costs, const KLocalizedString& tooltipTemplate)
{
    // restore selection if possible
    const auto oldData = combo->currentData();

    combo->clear();
    for (int i = 0, c = costs.numTypes(); i < c; ++i) {
        if (!costs.totalCost(i)) {
            continue;
        }
        const auto& typeName = costs.typeName(i);
        combo->addItem(typeName, QVariant::fromValue(i));
        combo->setItemData(i, tooltipTemplate.subs(typeName).toString(), Qt::ToolTipRole);
    }

    const auto index = combo->findData(oldData);
    if (index != -1) {
        combo->setCurrentIndex(index);
    }
}
}
