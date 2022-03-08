/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultstopdownpage.h"
#include "ui_resultstopdownpage.h"

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/hashmodel.h"
#include "models/treemodel.h"

ResultsTopDownPage::ResultsTopDownPage(FilterAndZoomStack* filterStack, PerfParser* parser,
                                       CostContextMenu* contextMenu, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsTopDownPage)
{
    ui->setupUi(this);

    auto topDownCostModel = new TopDownModel(this);
    ResultsUtil::setupTreeView(ui->topDownTreeView, contextMenu, ui->topDownSearch, topDownCostModel);
    ResultsUtil::setupCostDelegate(topDownCostModel, ui->topDownTreeView);
    ResultsUtil::setupContextMenu(ui->topDownTreeView, contextMenu, topDownCostModel, filterStack, this);

    connect(parser, &PerfParser::topDownDataAvailable, this,
            [this, topDownCostModel, parser](const Data::TopDownResults& data) {
                topDownCostModel->setData(data);
                ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->topDownTreeView, TopDownModel::NUM_BASE_COLUMNS);

                ResultsUtil::hideEmptyColumns(data.selfCosts, ui->topDownTreeView,
                                              TopDownModel::NUM_BASE_COLUMNS + data.inclusiveCosts.numTypes());
                ResultsUtil::hideTracepointColumns(data.selfCosts, ui->topDownTreeView,
                                                   TopDownModel::NUM_BASE_COLUMNS + data.inclusiveCosts.numTypes(),
                                                   parser->tracepointCostNames());

                // hide self cost columns for sched:sched_switch and off-CPU
                // quasi all rows will have a cost of 0%, and only the leaves will show
                // a non-zero value that is equal to the inclusive cost then
                const auto costs = data.inclusiveCosts.numTypes();
                const auto schedSwitchName = QLatin1String("sched:sched_switch");
                const auto offCpuName = PerfParser::tr("off-CPU Time");
                for (int i = 0; i < costs; ++i) {
                    const auto typeName = data.inclusiveCosts.typeName(i);
                    if (typeName == schedSwitchName || typeName == offCpuName) {
                        ui->topDownTreeView->hideColumn(topDownCostModel->selfCostColumn(i));
                    }
                }
            });
}

ResultsTopDownPage::~ResultsTopDownPage() = default;

void ResultsTopDownPage::clear()
{
    ui->topDownSearch->setText({});
}
