/*
  resultscallercalleepage.cpp

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

#include "resultscallercalleepage.h"
#include "ui_resultscallercalleepage.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QSortFilterProxyModel>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/callercalleemodel.h"
#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/treemodel.h"
#include "models/filterandzoomstack.h"

namespace {
template<typename Model>
Model* setupModelAndProxyForView(QTreeView* view)
{
    auto model = new Model(view);
    auto proxy = new QSortFilterProxyModel(model);
    proxy->setSourceModel(model);
    proxy->setSortRole(Model::SortRole);
    view->sortByColumn(Model::InitialSortColumn, Qt::DescendingOrder);
    view->setModel(proxy);
    ResultsUtil::setupHeaderView(view);
    ResultsUtil::setupCostDelegate(model, view);

    return model;
}

template<typename Model, typename Handler>
void connectCallerOrCalleeModel(QTreeView* view, CallerCalleeModel* callerCalleeCostModel, Handler handler)
{
    QObject::connect(view, &QTreeView::activated, view, [callerCalleeCostModel, handler](const QModelIndex& index) {
        const auto symbol = index.data(Model::SymbolRole).template value<Data::Symbol>();
        auto sourceIndex = callerCalleeCostModel->indexForKey(symbol);
        handler(sourceIndex);
    });
}
}

ResultsCallerCalleePage::ResultsCallerCalleePage(FilterAndZoomStack* filterStack, PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsCallerCalleePage)
{
    ui->setupUi(this);

    m_callerCalleeCostModel = new CallerCalleeModel(this);
    m_callerCalleeProxy = new QSortFilterProxyModel(this);
    m_callerCalleeProxy->setSourceModel(m_callerCalleeCostModel);
    m_callerCalleeProxy->setSortRole(CallerCalleeModel::SortRole);
    ResultsUtil::connectFilter(ui->callerCalleeFilter, m_callerCalleeProxy);
    ui->callerCalleeTableView->setSortingEnabled(true);
    ui->callerCalleeTableView->setModel(m_callerCalleeProxy);
    ResultsUtil::setupContextMenu(ui->callerCalleeTableView, m_callerCalleeCostModel, filterStack, this,
                                  {ResultsUtil::CallbackAction::OpenEditor, ResultsUtil::CallbackAction::SelectSymbol, ResultsUtil::CallbackAction::ViewDisassembly});
    ResultsUtil::setupHeaderView(ui->callerCalleeTableView);
    ResultsUtil::setupCostDelegate(m_callerCalleeCostModel, ui->callerCalleeTableView);

    connect(parser, &PerfParser::callerCalleeDataAvailable, this, [this](const Data::CallerCalleeResults& data) {
        m_callerCalleeCostModel->setResults(data);
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->callerCalleeTableView,
                                      CallerCalleeModel::NUM_BASE_COLUMNS);
        ResultsUtil::hideEmptyColumns(data.selfCosts, ui->callerCalleeTableView,
                                      CallerCalleeModel::NUM_BASE_COLUMNS + data.inclusiveCosts.numTypes());
        auto view = ui->callerCalleeTableView;
        view->sortByColumn(CallerCalleeModel::InitialSortColumn, view->header()->sortIndicatorOrder());
        view->setCurrentIndex(view->model()->index(0, 0, {}));
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->callersView, CallerModel::NUM_BASE_COLUMNS);
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->calleesView, CalleeModel::NUM_BASE_COLUMNS);
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->sourceMapView, SourceMapModel::NUM_BASE_COLUMNS);
    });

    auto calleesModel = setupModelAndProxyForView<CalleeModel>(ui->calleesView);
    auto callersModel = setupModelAndProxyForView<CallerModel>(ui->callersView);
    auto sourceMapModel = setupModelAndProxyForView<SourceMapModel>(ui->sourceMapView);

    auto selectCallerCaleeeIndex = [calleesModel, callersModel, sourceMapModel, this](const QModelIndex& index) {
        const auto costs = index.data(CallerCalleeModel::SelfCostsRole).value<Data::Costs>();
        const auto callees = index.data(CallerCalleeModel::CalleesRole).value<Data::CalleeMap>();
        calleesModel->setResults(callees, costs);
        const auto callers = index.data(CallerCalleeModel::CallersRole).value<Data::CallerMap>();
        callersModel->setResults(callers, costs);
        const auto sourceMap = index.data(CallerCalleeModel::SourceMapRole).value<Data::SourceLocationCostMap>();
        sourceMapModel->setResults(sourceMap, costs);
        if (index.model() == m_callerCalleeCostModel) {
            ui->callerCalleeTableView->setCurrentIndex(m_callerCalleeProxy->mapFromSource(index));
        }
    };
    connectCallerOrCalleeModel<CalleeModel>(ui->calleesView, m_callerCalleeCostModel, selectCallerCaleeeIndex);
    connectCallerOrCalleeModel<CallerModel>(ui->callersView, m_callerCalleeCostModel, selectCallerCaleeeIndex);
    ResultsUtil::setupContextMenu(ui->calleesView, calleesModel, filterStack, this,
                                  {ResultsUtil::CallbackAction::OpenEditor, ResultsUtil::CallbackAction::SelectSymbol, ResultsUtil::CallbackAction::ViewDisassembly});
    ResultsUtil::setupContextMenu(ui->callersView, callersModel, filterStack, this,
                                  {ResultsUtil::CallbackAction::OpenEditor, ResultsUtil::CallbackAction::SelectSymbol, ResultsUtil::CallbackAction::ViewDisassembly});

    ui->sourceMapView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->sourceMapView, &QTreeView::customContextMenuRequested, this,
            &ResultsCallerCalleePage::onSourceMapContextMenu);
    connect(ui->sourceMapView, &QTreeView::activated, this, &ResultsCallerCalleePage::onSourceMapActivated);

    connect(ui->callerCalleeTableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [selectCallerCaleeeIndex](const QModelIndex& current, const QModelIndex&) {
                if (current.isValid()) {
                    selectCallerCaleeeIndex(current);
                }
            });
}

ResultsCallerCalleePage::~ResultsCallerCalleePage() = default;

ResultsCallerCalleePage::SourceMapLocation
ResultsCallerCalleePage::toSourceMapLocation(const QString& location, const Data::Symbol& symbol) const
{
    const auto separator = location.lastIndexOf(QLatin1Char(':'));
    if (separator <= 0) {
        return {};
    }

    const auto fileName = location.leftRef(separator);
    const auto lineNumber = location.midRef(separator + 1).toInt();

    SourceMapLocation ret;
    auto resolvePath = [&ret, fileName, lineNumber](const QString& pathName) -> bool {
        const QString path = pathName + fileName;
        if (QFileInfo::exists(path)) {
            ret.path = path;
            ret.lineNumber = lineNumber;
            return true;
        }
        return false;
    };

    // also try to resolve paths relative to the module output folder
    // fixes a common issue with qmake builds that use relative paths
    const QString modulePath = QFileInfo(symbol.path).path() + QLatin1Char('/');

    resolvePath(m_sysroot) || resolvePath(m_sysroot + modulePath) || resolvePath(m_appPath)
        || resolvePath(m_appPath + modulePath);

    return ret;
}

ResultsCallerCalleePage::SourceMapLocation ResultsCallerCalleePage::toSourceMapLocation(const QModelIndex& index) const
{
    Q_ASSERT(qobject_cast<const SourceMapModel*>(index.model()));
    const auto location = index.data(SourceMapModel::LocationRole).toString();
    const auto symbol =
        ui->callerCalleeTableView->currentIndex().data(CallerCalleeModel::SymbolRole).value<Data::Symbol>();
    return toSourceMapLocation(location, symbol);
}

void ResultsCallerCalleePage::onSourceMapContextMenu(const QPoint& point)
{
    const auto index = ui->sourceMapView->indexAt(point);
    if (!index.isValid()) {
        return;
    }

    const auto location = toSourceMapLocation(index);
    if (!location) {
        return;
    }

    QMenu contextMenu;
    auto* viewCallerCallee = contextMenu.addAction(tr("Open in Editor"));
    auto* action = contextMenu.exec(QCursor::pos());
    if (action == viewCallerCallee) {
        emit navigateToCode(location.path, location.lineNumber, 0);
    }
}

void ResultsCallerCalleePage::onSourceMapActivated(const QModelIndex& index)
{
    const auto location = toSourceMapLocation(index);
    if (location) {
        emit navigateToCode(location.path, location.lineNumber, 0);
    } else {
        const auto locationStr = index.data(SourceMapModel::LocationRole).toString();
        emit navigateToCodeFailed(tr("Failed to find file for location '%1'.").arg(locationStr));
    }
}

void ResultsCallerCalleePage::setSysroot(const QString& path)
{
    m_sysroot = path;
}

void ResultsCallerCalleePage::setAppPath(const QString& path)
{
    m_appPath = path;
}

void ResultsCallerCalleePage::clear()
{
    ui->callerCalleeFilter->setText({});
}

void ResultsCallerCalleePage::jumpToCallerCallee(const Data::Symbol& symbol)
{
    auto callerCalleeIndex = m_callerCalleeProxy->mapFromSource(m_callerCalleeCostModel->indexForSymbol(symbol));
    ui->callerCalleeTableView->setCurrentIndex(callerCalleeIndex);
}

void ResultsCallerCalleePage::openEditor(const Data::Symbol& symbol)
{
    const auto callerCalleeIndex = m_callerCalleeProxy->mapFromSource(m_callerCalleeCostModel->indexForSymbol(symbol));
    const auto map = callerCalleeIndex.data(CallerCalleeModel::SourceMapRole).value<Data::SourceLocationCostMap>();

    auto it = std::find_if(map.keyBegin(), map.keyEnd(), [&symbol, this](const QString& locationStr) {
        const auto location = toSourceMapLocation(locationStr, symbol);
        if (location) {
            emit navigateToCode(location.path, location.lineNumber, 0);
            return true;
        }
        return false;
    });

    if (it == map.keyEnd()) {
        emit navigateToCodeFailed(
            tr("Failed to find location for symbol %1 in %2.").arg(symbol.prettySymbol, symbol.binary));
    }
}
