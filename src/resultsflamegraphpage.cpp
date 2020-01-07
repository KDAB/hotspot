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

#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QImageWriter>
#include <QTextStream>
#include <QMessageBox>

namespace {
QString imageFormatFilter()
{
    QString filter;
    {
        QTextStream stream(&filter);
        for (const auto format : QImageWriter::supportedImageFormats())
            stream << "*." << format.toLower() << ' ';
    }
    filter.chop(1); // remove trailing whitespace
    return filter;
}
}

ResultsFlameGraphPage::ResultsFlameGraphPage(FilterAndZoomStack* filterStack, PerfParser* parser, QMenu* exportMenu, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsFlameGraphPage)
{
    ui->setupUi(this);
    ui->flameGraph->setFilterStack(filterStack);

    connect(parser, &PerfParser::bottomUpDataAvailable, this,
            [this, exportMenu](const Data::BottomUpResults& data) {
                ui->flameGraph->setBottomUpData(data);
                m_exportAction = exportMenu->addAction(QIcon::fromTheme(QStringLiteral("image-x-generic")), tr("Flamegraph"));
                connect(m_exportAction, &QAction::triggered, this, [this]() {
                    const auto filter = tr("Images (%1);;SVG (*.svg)").arg(imageFormatFilter());
                    QString selectedFilter;
                    const auto fileName = QFileDialog::getSaveFileName(this, tr("Export Flamegraph"), {}, filter, &selectedFilter);
                    if (fileName.isEmpty())
                        return;
                    if (selectedFilter.contains(QStringLiteral("svg"))) {
                        ui->flameGraph->saveSvg(fileName);
                    } else {
                        QImageWriter writer(fileName);
                        if (!writer.write(ui->flameGraph->toImage())) {
                            QMessageBox::warning(this, tr("Export Failed"),
                                                tr("Failed to export flamegraph: %1").arg(writer.errorString()));
                        }
                    }
                });
            });

    connect(parser, &PerfParser::topDownDataAvailable, this,
            [this](const Data::TopDownResults& data) { ui->flameGraph->setTopDownData(data); });

    connect(ui->flameGraph, &FlameGraph::jumpToCallerCallee, this, &ResultsFlameGraphPage::jumpToCallerCallee);
}

void ResultsFlameGraphPage::clear()
{
    ui->flameGraph->clear();
    delete m_exportAction;
    m_exportAction = nullptr;
}

ResultsFlameGraphPage::~ResultsFlameGraphPage() = default;
