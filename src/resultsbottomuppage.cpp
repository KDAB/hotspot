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
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>

#include <KRecursiveFilterProxyModel>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"

namespace {
void stackCollapsedExport(QTextStream& file, int type, const Data::Costs &costs, const Data::BottomUp &node)
{
    if (!node.children.isEmpty()) {
        for (const auto& child : node.children)
            stackCollapsedExport(file, type, costs, child);
        return;
    }

    auto entry = &node;
    while (entry) {
        if (entry->symbol.symbol.isEmpty())
            file << '[' << entry->symbol.binary << ']';
        else
            file << Util::formatSymbol(entry->symbol);
        entry = entry->parent;
        if (entry)
            file << ';';
    }

    // leaf node, actually generate a line and write it to the file
    file << ' ';
    file << costs.cost(type, node.id);
    file << '\n';
}

void stackCollapsedExport(QFile& file, int type, const Data::BottomUpResults &results)
{
    QTextStream stream(&file);
    stackCollapsedExport(stream, type, results.costs, results.root);
}
}

ResultsBottomUpPage::ResultsBottomUpPage(FilterAndZoomStack* filterStack, PerfParser* parser, QMenu* exportMenu, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsBottomUpPage)
{
    ui->setupUi(this);

    auto bottomUpCostModel = new BottomUpModel(this);
    ResultsUtil::setupTreeView(ui->bottomUpTreeView, ui->bottomUpSearch, bottomUpCostModel);
    ResultsUtil::setupCostDelegate(bottomUpCostModel, ui->bottomUpTreeView);
    ResultsUtil::setupContextMenu(ui->bottomUpTreeView, bottomUpCostModel, filterStack,
                                  [this](const Data::Symbol& symbol) { emit jumpToCallerCallee(symbol); });

    auto topHotspotsProxy = new TopProxy(this);
    topHotspotsProxy->setSourceModel(bottomUpCostModel);

    connect(parser, &PerfParser::bottomUpDataAvailable, this,
            [this, bottomUpCostModel, exportMenu](const Data::BottomUpResults& data) {
                bottomUpCostModel->setData(data);
                ResultsUtil::hideEmptyColumns(data.costs, ui->bottomUpTreeView, BottomUpModel::NUM_BASE_COLUMNS);

                {
                    auto stackCollapsed = exportMenu->addMenu(QIcon::fromTheme(QStringLiteral("text-plain")), tr("Stack Collapsed"));
                    stackCollapsed->setToolTip(tr("Export data in textual form compatible with <tt>flamegraph.pl</tt>."));
                    for (int i = 0; i < data.costs.numTypes(); ++i) {
                        const auto costName = data.costs.typeName(i);
                        stackCollapsed->addAction(costName, [this, i, bottomUpCostModel, costName]() {
                            const auto fileName = QFileDialog::getSaveFileName(this, tr("Export %1 Data").arg(costName));
                            if (fileName.isEmpty())
                                return;
                            QFile file(fileName);
                            if (!file.open(QIODevice::Text | QIODevice::WriteOnly)) {
                                QMessageBox::warning(this, tr("Failed to export data"),
                                                     tr("Failed to export stack collapsed data:\n%1").arg(file.errorString()));
                                return;
                            }
                            stackCollapsedExport(file, i, bottomUpCostModel->results());
                        });
                    }
                }
            });
}

ResultsBottomUpPage::~ResultsBottomUpPage() = default;

void ResultsBottomUpPage::clear()
{
    ui->bottomUpSearch->setText({});
}
