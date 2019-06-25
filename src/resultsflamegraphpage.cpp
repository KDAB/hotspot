/*
  resultsflamegraphpage.cpp

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

#include "resultsflamegraphpage.h"
#include "ui_resultsflamegraphpage.h"

#include "parsers/perf/perfparser.h"

ResultsFlameGraphPage::ResultsFlameGraphPage(FilterAndZoomStack* filterStack, PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsFlameGraphPage)
{
    ui->setupUi(this);
    ui->flameGraph->setFilterStack(filterStack);

    connect(parser, &PerfParser::bottomUpDataAvailable, this,
            [this](const Data::BottomUpResults& data) { ui->flameGraph->setBottomUpData(data); });

    connect(parser, &PerfParser::topDownDataAvailable, this,
            [this](const Data::TopDownResults& data) { ui->flameGraph->setTopDownData(data); });

    connect(ui->flameGraph, &FlameGraph::jumpToCallerCallee, this, &ResultsFlameGraphPage::jumpToCallerCallee);
}

void ResultsFlameGraphPage::clear()
{
    ui->flameGraph->clear();
}

void ResultsFlameGraphPage::update()
{
    ui->flameGraph->update();
}

ResultsFlameGraphPage::~ResultsFlameGraphPage() = default;
