/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "resultssummarypage.h"
#include "ui_resultssummarypage.h"

#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QTextStream>

#include <KFormat>
#include <KLocalizedString>

#include "parsers/perf/perfparser.h"
#include "resultsutil.h"
#include "util.h"

#include "models/callercalleemodel.h"
#include "models/costdelegate.h"
#include "models/hashmodel.h"
#include "models/topproxy.h"
#include "models/treemodel.h"

ResultsSummaryPage::ResultsSummaryPage(FilterAndZoomStack* filterStack, PerfParser* parser,
                                       CostContextMenu* contextMenu, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ResultsSummaryPage)
{
    ui->setupUi(this);

    ui->parserErrorsBox->setVisible(false);

    auto bottomUpCostModel = new BottomUpModel(this);
    auto perLibraryModel = new PerLibraryModel(this);

    auto topHotspotsProxy = new TopProxy(this);
    topHotspotsProxy->setSourceModel(bottomUpCostModel);

    ui->topHotspotsTableView->setSortingEnabled(false);
    ui->topHotspotsTableView->setModel(topHotspotsProxy);
    ResultsUtil::setupCostDelegate<BottomUpModel>(bottomUpCostModel, ui->topHotspotsTableView);
    ResultsUtil::setupHeaderView(ui->topHotspotsTableView, contextMenu);
    ResultsUtil::setupContextMenu(ui->topHotspotsTableView, contextMenu, bottomUpCostModel, filterStack, this);

    auto topLibraryProxy = new TopProxy(this);
    topLibraryProxy->setSourceModel(perLibraryModel);
    topLibraryProxy->setCostColumn(PerLibraryModel::InitialSortColumn);
    topLibraryProxy->setNumBaseColumns(PerLibraryModel::NUM_BASE_COLUMNS);

    ui->topLibraryTreeView->setSortingEnabled(false);
    ui->topLibraryTreeView->setModel(topLibraryProxy);
    ResultsUtil::setupCostDelegate<PerLibraryModel>(perLibraryModel, ui->topLibraryTreeView);
    ResultsUtil::setupHeaderView(ui->topLibraryTreeView, contextMenu);
    ResultsUtil::setupContextMenu(ui->topLibraryTreeView, contextMenu, perLibraryModel, filterStack, this);

    connect(ui->eventSourceComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [topHotspotsProxy, this]() {
                topHotspotsProxy->setCostColumn(ui->eventSourceComboBox->currentData().toInt()
                                                + BottomUpModel::NUM_BASE_COLUMNS);
            });

    connect(ui->eventSourceComboBox_2, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [topLibraryProxy, this]() {
                topLibraryProxy->setCostColumn(ui->eventSourceComboBox_2->currentData().toInt()
                                               + PerLibraryModel::NUM_BASE_COLUMNS);
            });

    connect(parser, &PerfParser::bottomUpDataAvailable, this,
            [this, bottomUpCostModel, parser](const Data::BottomUpResults& data) {
                bottomUpCostModel->setData(data);
                ResultsUtil::hideEmptyColumns(data.costs, ui->topHotspotsTableView, BottomUpModel::NUM_BASE_COLUMNS);
                ResultsUtil::hideTracepointColumns(data.costs, ui->topHotspotsTableView,
                                                   BottomUpModel::NUM_BASE_COLUMNS, parser->tracepointCostNames());
                ResultsUtil::fillEventSourceComboBox(ui->eventSourceComboBox, data.costs,
                                                     tr("Show top hotspots for %1 events."));
            });

    connect(parser, &PerfParser::perLibraryDataAvailable, this,
            [this, perLibraryModel, parser](const Data::PerLibraryResults& data) {
                perLibraryModel->setData(data);
                ResultsUtil::hideEmptyColumns(data.costs, ui->topLibraryTreeView, PerLibraryModel::NUM_BASE_COLUMNS);
                ResultsUtil::hideTracepointColumns(data.costs, ui->topLibraryTreeView,
                                                   PerLibraryModel::NUM_BASE_COLUMNS, parser->tracepointCostNames());

                ResultsUtil::fillEventSourceComboBox(ui->eventSourceComboBox_2, data.costs,
                                                     tr("Show top hotspots for %1 events."));
            });

    auto parserErrorsModel = new QStringListModel(this);
    ui->parserErrorsView->setModel(parserErrorsModel);

    connect(parser, &PerfParser::summaryDataAvailable, this, [this, parserErrorsModel](const Data::Summary& data) {
        auto formatSummaryText = [](const QString& description, const QString& value) -> QString {
            return QString(QLatin1String("<tr><td>") + description + QLatin1String(": </td><td>") + value
                           + QLatin1String("</td></tr>"));
        };

        QString summaryText;
        {
            const auto indent = QLatin1String("&nbsp;&nbsp;&nbsp;&nbsp;");
            QTextStream stream(&summaryText);
            stream << "<qt><table>"
                   << formatSummaryText(tr("Command"),
                                        QLatin1String("<tt>") + data.command.toHtmlEscaped() + QLatin1String("</tt>"))
                   << formatSummaryText(tr("Run Time"), Util::formatTimeString(data.applicationTime.delta()));
            if (data.offCpuTime > 0 || data.onCpuTime > 0) {
                stream << formatSummaryText(indent + tr("On CPU Time"), Util::formatTimeString(data.onCpuTime))
                       << formatSummaryText(indent + tr("Off CPU Time"), Util::formatTimeString(data.offCpuTime));
            }
            stream << formatSummaryText(tr("Processes"), QString::number(data.processCount))
                   << formatSummaryText(tr("Threads"), QString::number(data.threadCount));
            if (data.offCpuTime > 0 || data.onCpuTime > 0) {
                stream
                    << formatSummaryText(indent + tr("Avg. Running"),
                                         Util::formatCostRelative(data.onCpuTime, data.applicationTime.delta() * 100))
                    << formatSummaryText(indent + tr("Avg. Sleeping"),
                                         Util::formatCostRelative(data.offCpuTime, data.applicationTime.delta() * 100));
            }
            stream << formatSummaryText(
                tr("Total Samples"),
                tr("%1 (%4)").arg(QString::number(data.sampleCount),
                                  Util::formatFrequency(data.sampleCount, data.applicationTime.delta())));
            for (const auto& costSummary : data.costs) {
                if (!costSummary.sampleCount) {
                    continue;
                }
                if (costSummary.unit == Data::Costs::Unit::Time) {
                    // we show the on/off CPU time already above
                    continue;
                }
                stream << formatSummaryText(
                    indent + costSummary.label.toHtmlEscaped(),
                    tr("%1 (%2 samples, %3% of total, %4)")
                        .arg(Util::formatCost(costSummary.totalPeriod), Util::formatCost(costSummary.sampleCount),
                             Util::formatCostRelative(costSummary.sampleCount, data.sampleCount),
                             Util::formatFrequency(costSummary.sampleCount, data.applicationTime.delta())));
                if ((costSummary.sampleCount * 1E9 / data.applicationTime.delta()) < 100) {
                    stream << formatSummaryText(indent + tr("<b>WARNING</b>"), tr("Sampling frequency below 100Hz"));
                }
            }
            stream << formatSummaryText(tr("Lost Events"), QString::number(data.lostEvents));
            stream << formatSummaryText(tr("Lost Chunks"), QString::number(data.lostChunks));
            stream << "</table></qt>";
        }
        ui->summaryLabel->setText(summaryText);

        QString systemInfoText;
        if (!data.hostName.isEmpty()) {
            KFormat format;
            QTextStream stream(&systemInfoText);
            stream << "<qt><table>" << formatSummaryText(tr("Host Name"), data.hostName)
                   << formatSummaryText(tr("Linux Kernel Version"), data.linuxKernelVersion)
                   << formatSummaryText(tr("Perf Version"), data.perfVersion)
                   << formatSummaryText(tr("CPU Description"), data.cpuDescription)
                   << formatSummaryText(tr("CPU ID"), data.cpuId)
                   << formatSummaryText(tr("CPU Architecture"), data.cpuArchitecture)
                   << formatSummaryText(tr("CPUs Online"), QString::number(data.cpusOnline))
                   << formatSummaryText(tr("CPUs Available"), QString::number(data.cpusAvailable))
                   << formatSummaryText(tr("CPU Sibling Cores"), data.cpuSiblingCores)
                   << formatSummaryText(tr("CPU Sibling Threads"), data.cpuSiblingThreads)
                   << formatSummaryText(
                          tr("Total Memory"),
                          format.formatByteSize(data.totalMemoryInKiB * 1024, 1, KFormat::MetricBinaryDialect))
                   << "</table></qt>";
        }
        ui->systemInfoGroupBox->setVisible(!systemInfoText.isEmpty());
        ui->systemInfoLabel->setText(systemInfoText);

        if (data.errors.isEmpty()) {
            ui->parserErrorsBox->setVisible(false);
        } else {
            parserErrorsModel->setStringList(data.errors);
            ui->parserErrorsBox->setVisible(true);
        }
    });
}

ResultsSummaryPage::~ResultsSummaryPage() = default;
