/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultsbottomuppage.h"
#include "ui_resultsbottomuppage.h"

#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTextStream>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"

#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/treemodel.h"

namespace {
void stackCollapsedExport(QTextStream& file, int type, const Data::Costs& costs, const Data::BottomUp& node)
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

void stackCollapsedExport(QFile& file, int type, const Data::BottomUpResults& results)
{
    QTextStream stream(&file);
    stackCollapsedExport(stream, type, results.costs, results.root);
}
}

ResultsBottomUpPage::ResultsBottomUpPage(FilterAndZoomStack* filterStack, PerfParser* parser,
                                         CostContextMenu* contextMenu, QMenu* exportMenu, QWidget* parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::ResultsBottomUpPage>())
{
    ui->setupUi(this);

    auto bottomUpCostModel = new BottomUpModel(this);
    ResultsUtil::setupTreeView(ui->bottomUpTreeView, contextMenu, ui->bottomUpSearch, bottomUpCostModel);
    ResultsUtil::setupCostDelegate(bottomUpCostModel, ui->bottomUpTreeView);
    ResultsUtil::setupContextMenu(ui->bottomUpTreeView, contextMenu, bottomUpCostModel, filterStack, this);

    connect(
        parser, &PerfParser::bottomUpDataAvailable, this,
        [this, bottomUpCostModel, exportMenu](const Data::BottomUpResults& data) {
            bottomUpCostModel->setData(data);
            ResultsUtil::hideEmptyColumns(data.costs, ui->bottomUpTreeView, BottomUpModel::NUM_BASE_COLUMNS);

            {
                auto stackCollapsed =
                    exportMenu->addMenu(QIcon::fromTheme(QStringLiteral("text-plain")), tr("Stack Collapsed"));
                stackCollapsed->setToolTip(tr("Export data in textual form compatible with <tt>flamegraph.pl</tt>."));
                for (int i = 0; i < data.costs.numTypes(); ++i) {
                    const auto costName = data.costs.typeName(i);
                    stackCollapsed->addAction(costName, this, [this, i, bottomUpCostModel, costName]() {
                        const auto fileName = QFileDialog::getSaveFileName(this, tr("Export %1 Data").arg(costName));
                        if (fileName.isEmpty())
                            return;
                        QFile file(fileName);
                        if (!file.open(QIODevice::Text | QIODevice::WriteOnly)) {
                            QMessageBox::warning(
                                this, tr("Failed to export data"),
                                tr("Failed to export stack collapsed data:\n%1").arg(file.errorString()));
                            return;
                        }
                        stackCollapsedExport(file, i, bottomUpCostModel->results());
                    });
                }
            }
        });

    ResultsUtil::setupResultsAggregation(ui->costAggregationComboBox);
}

ResultsBottomUpPage::~ResultsBottomUpPage() = default;

void ResultsBottomUpPage::clear()
{
    ui->bottomUpSearch->setText({});
}
