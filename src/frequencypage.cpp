/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// NOTE: QCustomPlot still uses foreach
#undef QT_NO_FOREACH
#include <qcustomplot.h>
#undef foreach
#define QT_NO_FOREACH

#include "frequencypage.h"

#include <KColorScheme>
#include <QDebug>

#include "parsers/perf/perfparser.h"
#include "ui_frequencypage.h"
#include "util.h"

namespace {
struct PlotData
{
    quint64 applicationStartTime = 0;
};

class TimeAxis : public QCPAxisTicker
{
public:
    ~TimeAxis() override = default;

protected:
    QString getTickLabel(double tick, const QLocale& /*locale*/, QChar /*formatChar*/, int /*precision*/) override
    {
        return Util::formatTimeString(tick);
    }
};
}

FrequencyPage::FrequencyPage(PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , m_plot(new QCustomPlot(this))
    , m_page(std::make_unique<Ui::FrequencyPage>())
{
    m_plot->axisRect()->setupFullAxesBox(true);

    updateColors();

    m_page->setupUi(this);

    auto oldWidget = m_page->layout->replaceWidget(m_page->plotWidget, m_plot);
    delete oldWidget;

    auto plotData = QSharedPointer<PlotData>::create();

    connect(parser, &PerfParser::summaryDataAvailable,
            [plotData](const Data::Summary& data) { plotData->applicationStartTime = data.applicationTime.start; });

    connect(parser, &PerfParser::frequencyDataAvailable, this, [this](const Data::FrequencyResults& results) {
        m_results = results;

        m_page->costSelectionCombobox->clear();

        QSet<QString> costs;
        for (const auto& coreData : std::as_const(m_results.cores)) {
            for (const auto& costData : coreData.costs) {
                if (!costs.contains(costData.costName)) {
                    costs.insert(costData.costName);
                    m_page->costSelectionCombobox->addItem(costData.costName);
                }
            }
        }
    });

    auto updateYAxis = [this]() {
        const auto hideOutliers = m_page->hideOutliers->isChecked();
        if (hideOutliers && m_upperWithoutOutliers) {
            m_plot->yAxis->setRangeUpper(m_upperWithoutOutliers);
        } else {
            m_plot->yAxis->rescale();
        }

        m_plot->yAxis->setRangeLower(0.);

        m_plot->replot(QCustomPlot::rpQueuedRefresh);
    };

    auto updateGraphs = [this, plotData, updateYAxis]() {
        m_plot->clearGraphs();
        const auto averagingWindowSize = m_page->averagingWindowSize->value();
        const auto selectedCost = m_page->costSelectionCombobox->currentText();
        const auto numCores = m_results.cores.size();
        quint32 core = 0;
        m_upperWithoutOutliers = 0;

        double sumCost = 0;
        double numEntries = 0;

        for (const auto& coreData : std::as_const(m_results.cores)) {
            for (const auto& costData : coreData.costs) {
                if (costData.costName != selectedCost) {
                    continue;
                }

                auto graph = m_plot->addGraph();
                graph->setLayer(QStringLiteral("main"));
                graph->setLineStyle(QCPGraph::lsNone);

                auto color =
                    QColor::fromHsv(static_cast<int>(255. * (static_cast<float>(core) / numCores)), 255, 255, 150);
                graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssSquare, color, color, 4));
                graph->setAdaptiveSampling(false);
                graph->setName(QLatin1String("%1 (CPU #%2)").arg(costData.costName, QString::number(core)));
                graph->addToLegend();
                graph->setVisible(true);

                const auto numValues = static_cast<int>(costData.values.size());
                const auto valuesStart = costData.values.begin();
                QVector<double> times((numValues + averagingWindowSize - 1) / averagingWindowSize);
                QVector<double> costs(times.size());
                for (int i = 0, j = 0; i < numValues; ++j, i += averagingWindowSize) {
                    const auto averageWindowStart = std::next(valuesStart, i);
                    const auto windowEndIndex = std::min(numValues, i + averagingWindowSize);
                    const auto averageWindowEnd = std::next(valuesStart, windowEndIndex);
                    const auto actualWindowSize = windowEndIndex - i;
                    const auto value = std::accumulate(averageWindowStart, averageWindowEnd, Data::FrequencyData {},
                                                       [](Data::FrequencyData lhs, Data::FrequencyData rhs) {
                                                           lhs.time += rhs.time;
                                                           lhs.cost += rhs.cost;
                                                           return lhs;
                                                       });
                    const auto time =
                        static_cast<double>(value.time) / actualWindowSize - plotData->applicationStartTime;
                    times[j] = time;
                    costs[j] = value.cost / actualWindowSize;
                    numEntries += actualWindowSize;
                    sumCost += value.cost;
                }
                graph->setData(times, costs, true);
            }

            ++core;
        }
        m_plot->xAxis->rescale();

        const auto avgCost = sumCost / numEntries;
        m_upperWithoutOutliers = avgCost * 1.1;

        updateYAxis();
    };

    connect(m_page->costSelectionCombobox, qOverload<int>(&QComboBox::currentIndexChanged), this, updateGraphs);
    connect(m_page->averagingWindowSize, qOverload<int>(&QSpinBox::valueChanged), this, updateGraphs);
    connect(m_page->hideOutliers, &QCheckBox::toggled, this, updateYAxis);

    m_plot->xAxis->setLabel(tr("Time"));
    m_plot->xAxis->setTicker(QSharedPointer<TimeAxis>(new TimeAxis()));
    m_plot->yAxis->setLabel(tr("Frequency [GHz]"));
    m_plot->legend->setVisible(true);
}

FrequencyPage::~FrequencyPage() = default;

void FrequencyPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        updateColors();
    }
}

void FrequencyPage::updateColors()
{
    const auto colorScheme = KColorScheme(QPalette::Active);

    const auto foreground = QPen(colorScheme.foreground().color());
    const auto background = colorScheme.background();

    for (auto* axis : {m_plot->xAxis, m_plot->yAxis, m_plot->xAxis2, m_plot->yAxis2}) {
        axis->setLabelColor(foreground.color());
        axis->setTickLabelColor(foreground.color());
        axis->setTickPen(foreground);
        axis->setBasePen(foreground);
        axis->setSubTickPen(foreground);
    }

    m_plot->legend->setBorderPen(foreground);
    m_plot->legend->setTextColor(foreground.color());
    m_plot->legend->setBrush(background);

    m_plot->setBackground(background);
}
