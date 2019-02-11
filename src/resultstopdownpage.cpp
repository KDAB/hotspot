/*
  resultstopdownpage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "resultstopdownpage.h"
#include "ui_resultstopdownpage.h"

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/hashmodel.h"
#include "models/treemodel.h"

ResultsTopDownPage::ResultsTopDownPage(FilterAndZoomStack* filterStack, PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsTopDownPage)
{
    ui->setupUi(this);

    auto topDownCostModel = new TopDownModel(this);
    ResultsUtil::setupTreeView(ui->topDownTreeView, ui->topDownSearch, topDownCostModel);
    ResultsUtil::setupCostDelegate(topDownCostModel, ui->topDownTreeView);
    ResultsUtil::setupContextMenu(ui->topDownTreeView, topDownCostModel, filterStack,
                                  [this](const Data::Symbol& symbol) { emit jumpToCallerCallee(symbol); });

    connect(parser, &PerfParser::topDownDataAvailable, this,
            [this, topDownCostModel](const Data::TopDownResults& data) {
                topDownCostModel->setData(data);
                ResultsUtil::hideEmptyColumns(data.inclusiveCosts, ui->topDownTreeView, TopDownModel::NUM_BASE_COLUMNS);
                ResultsUtil::hideEmptyColumns(data.selfCosts, ui->topDownTreeView,
                                              TopDownModel::NUM_BASE_COLUMNS + data.inclusiveCosts.numTypes());
            });
}

ResultsTopDownPage::~ResultsTopDownPage() = default;

void ResultsTopDownPage::clear()
{
    ui->topDownSearch->setText({});
}
