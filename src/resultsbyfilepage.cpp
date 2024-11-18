/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultsbyfilepage.h"
#include "ui_resultsbyfilepage.h"

#include <QDebug>
#include <QSortFilterProxyModel>

#include "costcontextmenu.h"
#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/byfilemodel.h"
#include "models/callercalleemodel.h"
#include "models/callercalleeproxy.h"
#include "models/filterandzoomstack.h"
#include "models/hashmodel.h"
#include "models/treemodel.h"

#include <QPushButton>
#include <QTemporaryFile>

#include "hotspot-config.h"

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
}

ResultsByFilePage::ResultsByFilePage(FilterAndZoomStack* filterStack, PerfParser* parser, CostContextMenu* contextMenu,
                                     QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ResultsByFilePage>())
{
    Q_UNUSED(filterStack);

    ui->setupUi(this);

    m_byFileCostModel = new ByFileModel(this);
    m_byFileProxy = new CallerCalleeProxy<ByFileModel>(this);
    m_byFileProxy->setSourceModel(m_byFileCostModel);
    m_byFileProxy->setSortRole(ByFileModel::SortRole);
    ResultsUtil::connectFilter(ui->byFileFilter, m_byFileProxy, ui->regexCheckBox);
    ui->byFileTableView->setSortingEnabled(true);
    ui->byFileTableView->setModel(m_byFileProxy);
    ResultsUtil::setupHeaderView(ui->byFileTableView, contextMenu);
    ResultsUtil::setupCostDelegate(m_byFileCostModel, ui->byFileTableView);

    connect(parser, &PerfParser::byFileDataAvailable, this, [this](const Data::ByFileResults& data) {
        m_byFileCostModel->setResults(data);
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->byFileTableView, ByFileModel::NUM_BASE_COLUMNS);

        ResultsUtil::hideEmptyColumns(data.selfCosts, ui->byFileTableView,
                                      ByFileModel::NUM_BASE_COLUMNS + data.inclusiveCosts.numTypes());
        ResultsUtil::hideTracepointColumns(data.selfCosts, ui->byFileTableView, ByFileModel::NUM_BASE_COLUMNS);
        auto view = ui->byFileTableView;
        view->setCurrentIndex(view->model()->index(0, 0, {}));
        ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->sourceMapView, SourceMapModel::NUM_BASE_COLUMNS);
        ResultsUtil::hideTracepointColumns(data.selfCosts, ui->sourceMapView, SourceMapModel::NUM_BASE_COLUMNS);
    });

    auto sourceMapModel = setupModelAndProxyForView<SourceMapModel>(ui->sourceMapView, contextMenu);

    auto selectByFileIndex = [sourceMapModel, this](const QModelIndex& index) {
        const auto costs = index.data(ByFileModel::SelfCostsRole).value<Data::Costs>();
        const auto sourceMap = index.data(ByFileModel::SourceMapRole).value<Data::SourceLocationCostMap>();
        sourceMapModel->setResults(sourceMap, costs);
        if (index.model() == m_byFileCostModel) {
            ui->byFileTableView->setCurrentIndex(m_byFileProxy->mapFromSource(index));
        }
    };

    ui->sourceMapView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->sourceMapView, &QTreeView::customContextMenuRequested, this,
            &ResultsByFilePage::onSourceMapContextMenu);

    connect(ui->byFileTableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [selectByFileIndex](const QModelIndex& current, const QModelIndex&) {
                if (current.isValid()) {
                    selectByFileIndex(current);
                }
            });

    ResultsUtil::setupResultsAggregation(ui->costAggregationComboBox);
}

ResultsByFilePage::~ResultsByFilePage() = default;

void ResultsByFilePage::clear()
{
    ui->byFileFilter->setText({});
}

void ResultsByFilePage::onSourceMapContextMenu(QPoint point)
{
    const auto sourceMapIndex = ui->sourceMapView->indexAt(point);
    if (!sourceMapIndex.isValid()) {
        return;
    }

    auto fileLine = sourceMapIndex.data(SourceMapModel::FileLineRole).value<Data::FileLine>();
    if (fileLine.isValid()) {
        emit openFileLineRequested(fileLine);
    }
}
