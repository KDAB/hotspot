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
    , m_model(new BottomUpModel(this))
    , m_exportMenu(exportMenu)
    , ui(new Ui::ResultsBottomUpPage)
{
    ui->setupUi(this);

    ResultsUtil::setupTreeViewDiff(ui->bottomUpTreeView, contextMenu, ui->bottomUpSearch, m_model);
    ResultsUtil::setupCostDelegate(m_model, ui->bottomUpTreeView);
    ResultsUtil::setupContextMenu(ui->bottomUpTreeView, contextMenu, m_model, filterStack, this);

    if (parser)
        connect(parser, &PerfParser::bottomUpDataAvailable, this, &ResultsBottomUpPage::setBottomUpResults);

    ResultsUtil::setupResultsAggregation(ui->costAggregationComboBox);
}

ResultsBottomUpPage::~ResultsBottomUpPage() = default;

void ResultsBottomUpPage::clear()
{
    ui->bottomUpSearch->setText({});
}

void ResultsBottomUpPage::setBottomUpResults(const Data::BottomUpResults& results)
{
    m_model->setData(results);
    ResultsUtil::hideEmptyColumns(results.costs, ui->bottomUpTreeView, BottomUpModel::NUM_BASE_COLUMNS);

    auto stackCollapsed = m_exportMenu->addMenu(QIcon::fromTheme(QStringLiteral("text-plain")), tr("Stack Collapsed"));
    stackCollapsed->setToolTip(tr("Export data in textual form compatible with <tt>flamegraph.pl</tt>."));
    for (int i = 0; i < results.costs.numTypes(); ++i) {
        const auto costName = results.costs.typeName(i);
        stackCollapsed->addAction(costName, this, [this, i, costName]() {
            const auto fileName = QFileDialog::getSaveFileName(this, tr("Export %1 Data").arg(costName));
            if (fileName.isEmpty())
                return;
            QFile file(fileName);
            if (!file.open(QIODevice::Text | QIODevice::WriteOnly)) {
                QMessageBox::warning(this, tr("Failed to export data"),
                                     tr("Failed to export stack collapsed data:\n%1").arg(file.errorString()));
                return;
            }
            stackCollapsedExport(file, i, m_model->results());
        });
    }
}
