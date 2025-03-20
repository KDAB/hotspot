/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <functional>

#include "models/costproxy.h"
#include <QFlags>

class QMenu;
class QTreeView;
class QComboBox;
class QLineEdit;
class QSortFilterProxyModel;
class QAbstractItemModel;
class QCheckBox;

namespace Data {
class Costs;
struct Symbol;
}

class FilterAndZoomStack;
class CostContextMenu;

namespace ResultsUtil {
void setupHeaderView(QTreeView* view, CostContextMenu* contextMenu);

void connectFilter(QLineEdit* filter, QSortFilterProxyModel* proxy, QCheckBox* regexCheckBox);

void setupTreeView(QTreeView* view, CostContextMenu* contextMenu, QLineEdit* filter, QCheckBox* regexSearchCheckBox,
                   QSortFilterProxyModel* model, int initialSortColumn, int sortRole);

template<typename Model>
void setupTreeView(QTreeView* view, CostContextMenu* costContextMenu, QLineEdit* filter, QCheckBox* regexSearchCheckBox,
                   Model* model)
{
    auto* proxy = new CostProxy<Model>(view);
    proxy->setSourceModel(model);
    setupTreeView(view, costContextMenu, filter, regexSearchCheckBox, qobject_cast<QSortFilterProxyModel*>(proxy),
                  Model::InitialSortColumn, Model::SortRole);
}

void setupCostDelegate(QAbstractItemModel* model, QTreeView* view, int sortRole, int totalCostRole, int numBaseColumns);

template<typename Model>
void setupCostDelegate(Model* model, QTreeView* view)
{
    setupCostDelegate(model, view, Model::SortRole, Model::TotalCostRole, Model::NUM_BASE_COLUMNS);
}

void addFilterActions(QMenu* menu, const Data::Symbol& symbol, FilterAndZoomStack* filterStack);

enum class CallbackAction
{
    ViewCallerCallee = 0x1,
    OpenEditor = 0x2,
    SelectSymbol = 0x4,
    ViewDisassembly = 0x8
};
Q_DECLARE_FLAGS(CallbackActions, CallbackAction)

void setupContextMenu(QTreeView* view, CostContextMenu* costContextMenu, int symbolRole,
                      FilterAndZoomStack* filterStack, CallbackActions actions,
                      const std::function<void(CallbackAction action, const Data::Symbol&)>& callback);

template<typename Model, typename Context>
void setupContextMenu(QTreeView* view, CostContextMenu* costContextMenu, Model* /*model*/,
                      FilterAndZoomStack* filterStack, Context* context,
                      CallbackActions actions = {CallbackAction::ViewCallerCallee, CallbackAction::OpenEditor,
                                                 CallbackAction::SelectSymbol, CallbackAction::ViewDisassembly})
{
    setupContextMenu(view, costContextMenu, Model::SymbolRole, filterStack, actions,
                     [context](ResultsUtil::CallbackAction action, const Data::Symbol& symbol) {
                         switch (action) {
                         case ResultsUtil::CallbackAction::ViewCallerCallee:
                             context->jumpToCallerCallee(symbol);
                             break;
                         case ResultsUtil::CallbackAction::OpenEditor:
                             context->openEditor(symbol);
                             break;
                         case ResultsUtil::CallbackAction::SelectSymbol:
                             context->selectSymbol(symbol);
                             break;
                         case ResultsUtil::CallbackAction::ViewDisassembly:
                             context->jumpToDisassembly(symbol);
                             break;
                         }
                     });
}

void hideEmptyColumns(const Data::Costs& costs, QTreeView* view, int numBaseColumns);

void hideTracepointColumns(const Data::Costs& costs, QTreeView* view, int numBaseColumns);

void fillEventSourceComboBox(QComboBox* combo, const Data::Costs& costs, const QString& tooltipTemplate);
void fillEventSourceComboBoxMultiSelect(QComboBox* combo, const Data::Costs& costs, const QString& tooltipTemplate);

void setupResultsAggregation(QComboBox* costAggregationComboBox);
}
