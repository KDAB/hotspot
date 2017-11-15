/*
  resultssummarypage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "resultssummarypage.h"
#include "ui_resultssummarypage.h"

#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QTextStream>

#include <KRecursiveFilterProxyModel>
#include <KLocalizedString>
#include <KFormat>

#include "parsers/perf/perfparser.h"
#include "util.h"
#include "resultsutil.h"

#include "models/summarydata.h"
#include "models/hashmodel.h"
#include "models/costdelegate.h"
#include "models/treemodel.h"
#include "models/topproxy.h"
#include "models/callercalleemodel.h"

ResultsSummaryPage::ResultsSummaryPage(PerfParser *parser, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ResultsSummaryPage)
{
    ui->setupUi(this);

    ui->lostMessage->setVisible(false);
    ui->parserErrorsBox->setVisible(false);

    auto bottomUpCostModel = new BottomUpModel(this);

    auto topHotspotsProxy = new TopProxy(this);
    topHotspotsProxy->setSourceModel(bottomUpCostModel);

    ui->topHotspotsTableView->setSortingEnabled(false);
    ui->topHotspotsTableView->setModel(topHotspotsProxy);
    ResultsUtil::setupCostDelegate<BottomUpModel> (bottomUpCostModel, ui->topHotspotsTableView);
    ResultsUtil::stretchFirstColumn(ui->topHotspotsTableView);

    connect(parser, &PerfParser::bottomUpDataAvailable,
            this, [this, bottomUpCostModel] (const Data::BottomUpResults& data) {
                bottomUpCostModel->setData(data);
                ResultsUtil::hideEmptyColumns(data.costs, ui->topHotspotsTableView, BottomUpModel::NUM_BASE_COLUMNS);
            });

    auto parserErrorsModel = new QStringListModel(this);
    ui->parserErrorsView->setModel(parserErrorsModel);

    connect(parser, &PerfParser::summaryDataAvailable,
            this, [this, parserErrorsModel] (const SummaryData& data) {
                auto formatSummaryText = [] (const QString& description, const QString& value) -> QString {
                    return QString(QLatin1String("<tr><td>") + description + QLatin1String(": </td><td>")
                                   + value + QLatin1String("</td></tr>"));
                };

                QString summaryText;
                {
                    QTextStream stream(&summaryText);
                    stream << "<qt><table>"
                           << formatSummaryText(tr("Command"), QLatin1String("<tt>") + data.command.toHtmlEscaped() + QLatin1String("</tt>"))
                           << formatSummaryText(tr("Run Time"), Util::formatTimeString(data.applicationRunningTime));
                    if (data.offCpuTime > 0 || data.onCpuTime > 0) {
                        stream << formatSummaryText(tr("On CPU Time"), Util::formatTimeString(data.onCpuTime))
                               << formatSummaryText(tr("Off CPU Time"), Util::formatTimeString(data.offCpuTime));
                    }
                    stream << formatSummaryText(tr("Processes"), QString::number(data.processCount))
                           << formatSummaryText(tr("Threads"), QString::number(data.threadCount))
                           << formatSummaryText(tr("Total Samples"), tr("%1 (%4)")
                                .arg(QString::number(data.sampleCount), Util::formatFrequency(data.sampleCount, data.applicationRunningTime)));
                    const auto indent = QLatin1String("&nbsp;&nbsp;&nbsp;&nbsp;");
                    for (const auto& costSummary : data.costs) {
                        stream << formatSummaryText(indent + costSummary.label.toHtmlEscaped(),
                                                    tr("%1 (%2 samples, %3% of total, %4)")
                                                        .arg(Util::formatCost(costSummary.totalPeriod),
                                                             Util::formatCost(costSummary.sampleCount),
                                                             Util::formatCostRelative(costSummary.sampleCount, data.sampleCount),
                                                             Util::formatFrequency(costSummary.sampleCount, data.applicationRunningTime)));
                        if ((costSummary.sampleCount * 1E9 / data.applicationRunningTime) < 100) {
                            stream << formatSummaryText(indent + tr("<b>WARNING</b>"), tr("Sampling frequency below 100Hz"));
                        }
                    }
                    stream << formatSummaryText(tr("Lost Chunks"), QString::number(data.lostChunks))
                           << "</table></qt>";
                }
                ui->summaryLabel->setText(summaryText);

                QString systemInfoText;
                if (!data.hostName.isEmpty()) {
                    KFormat format;
                    QTextStream stream(&systemInfoText);
                    stream << "<qt><table>"
                           << formatSummaryText(tr("Host Name"), data.hostName)
                           << formatSummaryText(tr("Linux Kernel Version"), data.linuxKernelVersion)
                           << formatSummaryText(tr("Perf Version"), data.perfVersion)
                           << formatSummaryText(tr("CPU Description"), data.cpuDescription)
                           << formatSummaryText(tr("CPU ID"), data.cpuId)
                           << formatSummaryText(tr("CPU Architecture"), data.cpuArchitecture)
                           << formatSummaryText(tr("CPUs Online"), QString::number(data.cpusOnline))
                           << formatSummaryText(tr("CPUs Available"), QString::number(data.cpusAvailable))
                           << formatSummaryText(tr("CPU Sibling Cores"), data.cpuSiblingCores)
                           << formatSummaryText(tr("CPU Sibling Threads"), data.cpuSiblingThreads)
                           << formatSummaryText(tr("Total Memory"), format.formatByteSize(data.totalMemoryInKiB * 1024, 1, KFormat::MetricBinaryDialect))
                           << "</table></qt>";
                }
                ui->systemInfoGroupBox->setVisible(!systemInfoText.isEmpty());
                ui->systemInfoLabel->setText(systemInfoText);

                if (data.lostChunks > 0) {
                    ui->lostMessage->setText(i18np("Lost one chunk - Check IO/CPU overload!",
                                                   "Lost %1 chunks - Check IO/CPU overload!",
                                                   data.lostChunks));
                    ui->lostMessage->setVisible(true);
                } else {
                    ui->lostMessage->setVisible(false);
                }

                if (data.errors.isEmpty()) {
                    ui->parserErrorsBox->setVisible(false);
                } else {
                    parserErrorsModel->setStringList(data.errors);
                    ui->parserErrorsBox->setVisible(true);
                }
            });
}

ResultsSummaryPage::~ResultsSummaryPage() = default;
