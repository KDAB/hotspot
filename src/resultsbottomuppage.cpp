/*
  resultsbottomuppage.cpp

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

#include "resultsbottomuppage.h"
#include "ui_resultsbottomuppage.h"

#include <QMenu>

#include <KRecursiveFilterProxyModel>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"

ResultsBottomUpPage::ResultsBottomUpPage(PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsBottomUpPage)
{
    ui->setupUi(this);

    auto bottomUpCostModel = new BottomUpModel(this);
    ResultsUtil::setupTreeView(ui->bottomUpTreeView, ui->bottomUpSearch, bottomUpCostModel);
    ResultsUtil::setupCostDelegate(bottomUpCostModel, ui->bottomUpTreeView);
    ResultsUtil::setupContextMenu(ui->bottomUpTreeView, bottomUpCostModel,
                                  [this](const Data::Symbol& symbol) { emit jumpToCallerCallee(symbol); });

    auto topHotspotsProxy = new TopProxy(this);
    topHotspotsProxy->setSourceModel(bottomUpCostModel);

    connect(parser, &PerfParser::bottomUpDataAvailable, this,
            [this, bottomUpCostModel](const Data::BottomUpResults& data) {
                bottomUpCostModel->setData(data);
                ResultsUtil::hideEmptyColumns(data.costs, ui->bottomUpTreeView, BottomUpModel::NUM_BASE_COLUMNS);
            });
}

ResultsBottomUpPage::~ResultsBottomUpPage() = default;
