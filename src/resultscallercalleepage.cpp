/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultscallercalleepage.h"
#include "ui_resultscallercalleepage.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QSortFilterProxyModel>

#include "costcontextmenu.h"
#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/callercalleemodel.h"
#include "models/callercalleeproxy.h"
#include "models/disassemblyoutput.h"
#include "models/filterandzoomstack.h"
#include "models/hashmodel.h"
#include "models/treemodel.h"

#include <QPushButton>
#include <QTemporaryFile>

#include "hotspot-config.h"

#if KGraphViewerPart_FOUND
#include "callgraphwidget.h"
#endif

namespace {
QSortFilterProxyModel* createProxy(SourceMapModel* model)
{
    return new SourceMapProxy(model);
}

template<typename Model>
QSortFilterProxyModel* createProxy(Model* model)
{
    return new CallerCalleeProxy<Model>(model);
}

template<typename Model>
Model* setupModelAndProxyForView(QTreeView* view, CostContextMenu* contextMenu)
{
    auto model = new Model(view);
    auto proxy = createProxy(model);
    proxy->setSourceModel(model);
    proxy->setSortRole(Model::SortRole);
    view->setModel(proxy);
    ResultsUtil::setupHeaderView(view, contextMenu);
    ResultsUtil::setupCostDelegate(model, view);
    view->sortByColumn(Model::InitialSortColumn, Qt::DescendingOrder);

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

ResultsCallerCalleePage::ResultsCallerCalleePage(FilterAndZoomStack* filterStack, PerfParser* parser,
                                                 CostContextMenu* contextMenu, QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ResultsCallerCalleePage>())
{
    ui->setupUi(this);

    m_callerCalleeCostModel = new CallerCalleeModel(this);
    m_callerCalleeProxy = new CallerCalleeProxy<CallerCalleeModel>(this);
    m_callerCalleeProxy->setSourceModel(m_callerCalleeCostModel);
    m_callerCalleeProxy->setSortRole(CallerCalleeModel::SortRole);
    ResultsUtil::connectFilter(ui->callerCalleeFilter, m_callerCalleeProxy, ui->regexCheckBox);
    ui->callerCalleeTableView->setSortingEnabled(true);
    ui->callerCalleeTableView->setModel(m_callerCalleeProxy);
    ResultsUtil::setupContextMenu(ui->callerCalleeTableView, contextMenu, m_callerCalleeCostModel, filterStack, this,
                                  {ResultsUtil::CallbackAction::OpenEditor, ResultsUtil::CallbackAction::SelectSymbol,
                                   ResultsUtil::CallbackAction::ViewDisassembly});
    ResultsUtil::setupHeaderView(ui->callerCalleeTableView, contextMenu);
    ResultsUtil::setupCostDelegate(m_callerCalleeCostModel, ui->callerCalleeTableView);

    connect(parser, &PerfParser::callerCalleeDataAvailable, this, [this](const Data::CallerCalleeResults& data) {
        m_callerCalleeCostModel->setResults(data);
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->callerCalleeTableView,
                                      CallerCalleeModel::NUM_BASE_COLUMNS);

        ResultsUtil::hideEmptyColumns(data.selfCosts, ui->callerCalleeTableView,
                                      CallerCalleeModel::NUM_BASE_COLUMNS + data.inclusiveCosts.numTypes());
        ResultsUtil::hideTracepointColumns(data.selfCosts, ui->callerCalleeTableView,
                                           CallerCalleeModel::NUM_BASE_COLUMNS);
        auto view = ui->callerCalleeTableView;
        view->setCurrentIndex(view->model()->index(0, 0, {}));
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->callersView, CallerModel::NUM_BASE_COLUMNS);
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->calleesView, CalleeModel::NUM_BASE_COLUMNS);
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->sourceMapView, SourceMapModel::NUM_BASE_COLUMNS);
        ResultsUtil::hideTracepointColumns(data.selfCosts, ui->sourceMapView, SourceMapModel::NUM_BASE_COLUMNS);

#if KGraphViewerPart_FOUND
        if (m_callgraph) {
            m_callgraph->setResults(data);
        }
#endif
    });

#if KGraphViewerPart_FOUND
    m_callgraph = CallgraphWidget::createCallgraphWidget({}, this);
    if (m_callgraph) {
        ui->splitter_2->addWidget(m_callgraph);
    }
#endif

    auto calleesModel = setupModelAndProxyForView<CalleeModel>(ui->calleesView, contextMenu);
    auto callersModel = setupModelAndProxyForView<CallerModel>(ui->callersView, contextMenu);
    auto sourceMapModel = setupModelAndProxyForView<SourceMapModel>(ui->sourceMapView, contextMenu);

    auto selectCallerCalleeIndex = [calleesModel, callersModel, sourceMapModel, this](const QModelIndex& index) {
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
#if KGraphViewerPart_FOUND
        if (m_callgraph) {
            m_callgraph->selectSymbol(index.data(CallerCalleeModel::SymbolRole).value<Data::Symbol>());
        }
#endif
    };
    connectCallerOrCalleeModel<CalleeModel>(ui->calleesView, m_callerCalleeCostModel, selectCallerCalleeIndex);
    connectCallerOrCalleeModel<CallerModel>(ui->callersView, m_callerCalleeCostModel, selectCallerCalleeIndex);
    ResultsUtil::setupContextMenu(ui->calleesView, contextMenu, calleesModel, filterStack, this,
                                  {ResultsUtil::CallbackAction::OpenEditor, ResultsUtil::CallbackAction::SelectSymbol,
                                   ResultsUtil::CallbackAction::ViewDisassembly});
    ResultsUtil::setupContextMenu(ui->callersView, contextMenu, callersModel, filterStack, this,
                                  {ResultsUtil::CallbackAction::OpenEditor, ResultsUtil::CallbackAction::SelectSymbol,
                                   ResultsUtil::CallbackAction::ViewDisassembly});

#if KGraphViewerPart_FOUND
    if (m_callgraph) {
        connect(m_callgraph, &CallgraphWidget::clickedOn, this,
                [this, selectCallerCalleeIndex](const Data::Symbol& symbol) {
                    const auto index = m_callerCalleeCostModel->indexForKey(symbol);
                    selectCallerCalleeIndex(index);
                });
    }
#endif

    ui->sourceMapView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->sourceMapView, &QTreeView::customContextMenuRequested, this,
            &ResultsCallerCalleePage::onSourceMapContextMenu);

    connect(ui->callerCalleeTableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [selectCallerCalleeIndex](const QModelIndex& current, const QModelIndex&) {
                if (current.isValid()) {
                    selectCallerCalleeIndex(current);
                }
            });

    ResultsUtil::setupResultsAggregation(ui->costAggregationComboBox);
}

ResultsCallerCalleePage::~ResultsCallerCalleePage() = default;

ResultsCallerCalleePage::SourceMapLocation ResultsCallerCalleePage::toSourceMapLocation(const Data::FileLine& fileLine,
                                                                                        const QString& binaryPath) const
{
    if (!fileLine.isValid()) {
        return {};
    }

    SourceMapLocation ret;
    ret.binaryPath = binaryPath;

    auto resolvePath = [&ret, &fileLine](const QString& pathName) -> bool {
        const QString path = pathName + fileLine.file;
        if (QFileInfo::exists(path)) {
            ret.path = path;
            ret.lineNumber = fileLine.line;
            return true;
        }
        return false;
    };

    // also try to resolve paths relative to the module output folder
    // fixes a common issue with qmake builds that use relative paths
    const QString modulePath = QFileInfo(binaryPath).path() + QLatin1Char('/');

    resolvePath(m_sysroot) || resolvePath(m_sysroot + modulePath) || resolvePath(m_appPath)
        || resolvePath(m_appPath + modulePath);

    return ret;
}

ResultsCallerCalleePage::SourceMapLocation ResultsCallerCalleePage::toSourceMapLocation(const QModelIndex& index) const
{
    const auto fileLine = index.data(SourceMapModel::FileLineRole).value<Data::FileLine>();
    const auto symbol =
        ui->callerCalleeTableView->currentIndex().data(CallerCalleeModel::SymbolRole).value<Data::Symbol>();
    return toSourceMapLocation(fileLine, symbol.path);
}

void ResultsCallerCalleePage::openFileLineRequested(const Data::FileLine& fileLine)
{
    // TODO: how should we try to find a symbol here to resolve relative paths?
    showSourceMapContextMenu(toSourceMapLocation(fileLine, {}), {});
}

void ResultsCallerCalleePage::onSourceMapContextMenu(QPoint point)
{
    const auto sourceMapIndex = ui->sourceMapView->indexAt(point);
    if (!sourceMapIndex.isValid()) {
        return;
    }

    // fetch current symbol from callerCalleeView to check if we can disassemble it
    const auto symbol =
        ui->callerCalleeTableView->currentIndex().data(CallerCalleeModel::SymbolRole).value<Data::Symbol>();
    showSourceMapContextMenu(toSourceMapLocation(sourceMapIndex), symbol);
}

void ResultsCallerCalleePage::showSourceMapContextMenu(const SourceMapLocation& location, const Data::Symbol& symbol)
{
    if (!location)
        return;

    QMenu contextMenu;
    auto* viewCallerCallee = contextMenu.addAction(tr("Open in Editor"));
    connect(viewCallerCallee, &QAction::triggered, this,
            [this, location] { emit navigateToCode(location.path, location.lineNumber, 0); });

    auto disassemblyAction = contextMenu.addAction(tr("Disassembly"));
    disassemblyAction->setEnabled(symbol.canDisassemble());
    connect(disassemblyAction, &QAction::triggered, this,
            [this, symbol, location] { emit jumpToSourceCode(symbol, {location.path, location.lineNumber}); });

    contextMenu.exec(QCursor::pos());
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

    auto it = std::find_if(map.keyBegin(), map.keyEnd(), [&symbol, this](const Data::FileLine& fileLine) {
        const auto location = toSourceMapLocation(fileLine, symbol.path);
        if (location) {
            auto settings = Settings::instance();
            const auto colon = QLatin1Char(':');
            auto remappedSourceFile =
                findSourceCodeFile(location.path, settings->sourceCodePaths().split(colon), settings->sysroot());
            emit navigateToCode(remappedSourceFile, location.lineNumber, 0);
            return true;
        }
        return false;
    });

    if (it == map.keyEnd()) {
        emit navigateToCodeFailed(
            tr("Failed to find location for symbol %1 in %2.").arg(symbol.prettySymbol, symbol.binary));
    }
}
