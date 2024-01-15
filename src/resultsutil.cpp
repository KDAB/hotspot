/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultsutil.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QTreeView>

#include <QStandardItemModel>

#include "models/costdelegate.h"
#include "models/data.h"
#include "models/filterandzoomstack.h"

#include "costcontextmenu.h"
#include "costheaderview.h"
#include "settings.h"

namespace ResultsUtil {

void setupHeaderView(QTreeView* view, CostContextMenu* contextMenu)
{
    view->setHeader(new CostHeaderView(contextMenu, view));
}

void connectFilter(QLineEdit* filter, QSortFilterProxyModel* proxy, QCheckBox* regexCheckBox)
{
    Q_ASSERT(regexCheckBox);
    auto* timer = new QTimer(filter);
    timer->setSingleShot(true);

    filter->setClearButtonEnabled(true);
    filter->setPlaceholderText(QCoreApplication::translate("Util", "Search"));

    proxy->setFilterKeyColumn(-1);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    auto setFilterNeedle = [filter, proxy, regexCheckBox]() {
        auto useRegex = regexCheckBox->isChecked();
        const auto needle = filter->text();
        proxy->setFilterRegularExpression(useRegex ? needle : QRegularExpression::escape(needle));
    };

    QObject::connect(timer, &QTimer::timeout, proxy, setFilterNeedle);
    if (regexCheckBox) {
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
        QObject::connect(regexCheckBox, &QCheckBox::stateChanged, proxy, setFilterNeedle);
#else
        QObject::connect(regexCheckBox, &QCheckBox::checkStateChanged, proxy, setFilterNeedle);
#endif
    }
    QObject::connect(filter, &QLineEdit::textChanged, timer, [timer]() { timer->start(300); });
}

void setupTreeView(QTreeView* view, CostContextMenu* contextMenu, QLineEdit* filter, QCheckBox* regexSearchCheckbox,
                   QSortFilterProxyModel* model, int initialSortColumn, int sortRole)
{
    model->setSortRole(sortRole);
    connectFilter(filter, model, regexSearchCheckbox);

    view->setModel(model);
    setupHeaderView(view, contextMenu);
    view->sortByColumn(initialSortColumn, Qt::DescendingOrder);
}

void addFilterActions(QMenu* menu, const Data::Symbol& symbol, FilterAndZoomStack* filterStack)
{
    if (symbol.isValid()) {
        auto filterActions = filterStack->actions();

        // don't include symbol-related entries for binary-only symbols (like in Top Hotspots Per File)
        if (!symbol.symbol.isEmpty()) {
            auto symbolFilter = QVariant::fromValue(symbol);

            filterActions.filterInBySymbol->setData(symbolFilter);
            filterActions.filterOutBySymbol->setData(symbolFilter);

            menu->addAction(filterActions.filterInBySymbol);
            menu->addAction(filterActions.filterOutBySymbol);
            menu->addSeparator();
        }

        // don't include binary-related entries when we don't have this information
        if (!symbol.binary.isEmpty()) {
            auto binaryFilter = QVariant::fromValue(symbol.binary);

            filterActions.filterInByBinary->setData(binaryFilter);
            filterActions.filterOutByBinary->setData(binaryFilter);

            menu->addAction(filterActions.filterInByBinary);
            menu->addAction(filterActions.filterOutByBinary);
            menu->addSeparator();
        }
    }

    menu->addAction(filterStack->actions().filterOut);
    menu->addAction(filterStack->actions().resetFilter);
}

void setupContextMenu(QTreeView* view, CostContextMenu* costContextMenu, int symbolRole,
                      FilterAndZoomStack* filterStack, CallbackActions actions,
                      const std::function<void(CallbackAction action, const Data::Symbol&)>& callback)
{
    QObject::connect(costContextMenu, &CostContextMenu::hiddenColumnsChanged, view,
                     [view, costContextMenu] { costContextMenu->hideColumns(view); });

    view->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(view, &QTreeView::customContextMenuRequested, view, [=](QPoint point) {
        const auto index = view->indexAt(point);
        const auto symbol = index.data(symbolRole).value<Data::Symbol>();

        QMenu contextMenu;
        if (callback && symbol.isValid() && actions) {
            if (actions.testFlag(CallbackAction::ViewCallerCallee)) {
                auto* viewCallerCallee =
                    contextMenu.addAction(QCoreApplication::translate("Util", "View Caller/Callee"));
                QObject::connect(viewCallerCallee, &QAction::triggered, &contextMenu,
                                 [symbol, callback]() { callback(CallbackAction::ViewCallerCallee, symbol); });
                viewCallerCallee->setEnabled(symbol.canDisassemble());
            }
            if (actions.testFlag(CallbackAction::OpenEditor)) {
                auto* openEditorAction = contextMenu.addAction(QCoreApplication::translate("Util", "Open in Editor"));
                QObject::connect(openEditorAction, &QAction::triggered, &contextMenu,
                                 [symbol, callback]() { callback(CallbackAction::OpenEditor, symbol); });
                openEditorAction->setEnabled(symbol.canDisassemble());
            }
            if (actions.testFlag(CallbackAction::ViewDisassembly)) {
                auto* viewDisassembly = contextMenu.addAction(QCoreApplication::translate("Util", "Disassembly"));
                QObject::connect(viewDisassembly, &QAction::triggered, &contextMenu,
                                 [symbol, callback]() { callback(CallbackAction::ViewDisassembly, symbol); });
                viewDisassembly->setEnabled(symbol.canDisassemble());
            }
            contextMenu.addSeparator();
        }

        costContextMenu->addToMenu(view->header(),
                                   contextMenu.addMenu(QCoreApplication::translate("Util", "Visible Columns")));
        contextMenu.addSeparator();

        addFilterActions(&contextMenu, symbol, filterStack);

        if (!contextMenu.actions().isEmpty()) {
            contextMenu.exec(QCursor::pos());
        }
    });

    if (actions.testFlag(ResultsUtil::CallbackAction::SelectSymbol)) {
        QObject::connect(view->selectionModel(), &QItemSelectionModel::currentRowChanged, view,
                         [=](const QModelIndex& current) {
                             const auto symbol = current.data(symbolRole).value<Data::Symbol>();
                             callback(CallbackAction::SelectSymbol, symbol);
                         });
    }
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

void hideTracepointColumns(const Data::Costs& costs, QTreeView* view, int numBaseColumns)
{
    for (int i = 0, c = costs.numTypes(); i < c; i++) {
        const auto unit = costs.unit(i);
        switch (unit) {
        case Data::Costs::Unit::Time:
        case Data::Costs::Unit::Tracepoint:
            view->hideColumn(numBaseColumns + i);
        case Data::Costs::Unit::Unknown:
            break;
        }
    }
}

void fillEventSourceComboBox(QComboBox* combo, const Data::Costs& costs, const QString& tooltipTemplate)
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
        combo->setItemData(i, tooltipTemplate.arg(typeName), Qt::ToolTipRole);
    }

    const auto index = combo->findData(oldData);
    if (index != -1) {
        combo->setCurrentIndex(index);
    }
}

void fillEventSourceComboBoxMultiSelect(QComboBox* combo, const Data::Costs& costs, const QString& /*tooltipTemplate*/)
{
    // restore selection if possible
    const auto oldData = combo->currentData();

    combo->clear();

    auto model = new QStandardItemModel(costs.numTypes(), 1, combo);
    int itemCounter = 0;
    for (int costId = 0, c = costs.numTypes(); costId < c; costId++) {
        if (!costs.totalCost(costId)) {
            continue;
        }

        auto item = new QStandardItem(costs.typeName(costId));
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setData(Qt::Checked, Qt::CheckStateRole);
        item->setData(costId, Qt::UserRole + 1);
        model->setItem(itemCounter, item);
        itemCounter++;
    }
    model->setRowCount(itemCounter);
    combo->setModel(model);

    const auto index = combo->findData(oldData);
    if (index != -1) {
        combo->setCurrentIndex(index);
    }
}

void setupResultsAggregation(QComboBox* costAggregationComboBox)
{
    struct AggregationType
    {
        QString name;
        QString tooltip;
        Settings::CostAggregation aggregation;
    };

    const AggregationType types[] = {
        {QCoreApplication::translate("Util", "Symbol"),
         QCoreApplication::translate("Util",
                                     "Disable grouping and aggregate costs over all threads, processes and CPUs."),
         Settings::CostAggregation::BySymbol},
        {QCoreApplication::translate("Util", "Thread"),
         QCoreApplication::translate("Util",
                                     "Group events by thread id and aggregate costs separately for each thread."),
         Settings::CostAggregation::ByThread},
        {QCoreApplication::translate("Util", "Process"),
         QCoreApplication::translate("Util",
                                     "Group events by process id and aggregate costs separately for each process."),
         Settings::CostAggregation::ByProcess},
        {QCoreApplication::translate("Util", "CPU"),
         QCoreApplication::translate("Util", "Group events by CPU id and aggregate costs separately for each CPU."),
         Settings::CostAggregation::ByCPU}};
    for (const auto& aggregationType : types) {
        costAggregationComboBox->addItem(aggregationType.name, QVariant::fromValue(aggregationType.aggregation));
        costAggregationComboBox->setItemData(costAggregationComboBox->count() - 1, aggregationType.tooltip,
                                             Qt::ToolTipRole);
    }

    auto updateCostAggregation = [costAggregationComboBox](Settings::CostAggregation costAggregation) {
        auto idx = costAggregationComboBox->findData(QVariant::fromValue(costAggregation));
        Q_ASSERT(idx != -1);
        costAggregationComboBox->setCurrentIndex(idx);
    };
    updateCostAggregation(Settings::instance()->costAggregation());
    QObject::connect(Settings::instance(), &Settings::costAggregationChanged, costAggregationComboBox,
                     updateCostAggregation);

    QObject::connect(costAggregationComboBox, qOverload<int>(&QComboBox::currentIndexChanged), Settings::instance(),
                     [costAggregationComboBox] {
                         const auto aggregation =
                             costAggregationComboBox->currentData().value<Settings::CostAggregation>();
                         Settings::instance()->setCostAggregation(aggregation);
                     });
}
}
