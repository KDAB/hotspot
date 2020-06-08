/*
  resultsutil.h

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

#pragma once

#include <functional>

#include <QFlags>

class QMenu;
class QTreeView;
class QComboBox;
class QLineEdit;
class QSortFilterProxyModel;
class QAbstractItemModel;
class KLocalizedString;

namespace Data {
class Costs;
struct Symbol;
}

class FilterAndZoomStack;

namespace ResultsUtil {
void setupHeaderView(QTreeView* view);

void connectFilter(QLineEdit *filter, QSortFilterProxyModel *proxy);

void setupTreeView(QTreeView* view, QLineEdit* filter, QAbstractItemModel* model, int initialSortColumn,
                   int sortRole, int filterRole);

template<typename Model>
void setupTreeView(QTreeView* view, QLineEdit* filter, Model* model)
{
    setupTreeView(view, filter, model, Model::InitialSortColumn, Model::SortRole, Model::FilterRole);
}

void setupCostDelegate(QAbstractItemModel* model, QTreeView* view, int sortRole, int totalCostRole, int numBaseColumns);

template<typename Model>
void setupCostDelegate(Model* model, QTreeView* view)
{
    setupCostDelegate(model, view, Model::SortRole, Model::TotalCostRole, Model::NUM_BASE_COLUMNS);
}

void addFilterActions(QMenu* menu, const Data::Symbol &symbol, FilterAndZoomStack* filterStack);

enum class CallbackAction
{
    ViewCallerCallee = 0x1,
    OpenEditor = 0x2,
};
Q_DECLARE_FLAGS(CallbackActions, CallbackAction)

void setupContextMenu(QTreeView* view, int symbolRole, FilterAndZoomStack* filterStack, CallbackActions actions,
                      std::function<void(CallbackAction action, const Data::Symbol&)> callback);

template<typename Model, typename Context>
void setupContextMenu(QTreeView* view, Model* /*model*/, FilterAndZoomStack* filterStack, Context* context,
                      CallbackActions actions = {CallbackAction::ViewCallerCallee, CallbackAction::OpenEditor})
{
    setupContextMenu(view, Model::SymbolRole, filterStack, actions,
                     [context](ResultsUtil::CallbackAction action, const Data::Symbol& symbol) {
                         switch (action) {
                         case ResultsUtil::CallbackAction::ViewCallerCallee:
                             context->jumpToCallerCallee(symbol);
                             break;
                         case ResultsUtil::CallbackAction::OpenEditor:
                             context->openEditor(symbol);
                             break;
                         }
                     });
}

void hideEmptyColumns(const Data::Costs& costs, QTreeView* view, int numBaseColumns);

void fillEventSourceComboBox(QComboBox* combo, const Data::Costs& costs, const KLocalizedString& tooltipTemplate);
}
