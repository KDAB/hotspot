/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "frequencypage.h"

#include <qcustomplot.h>
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
    TimeAxis()
        : QCPAxisTicker()
    {
    }

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
    , m_page(new Ui::FrequencyPage)
{
    m_plot->axisRect()->setupFullAxesBox(true);

    updateColors();

    m_page->setupUi(this);

    m_page->layout->replaceWidget(m_page->plotWidget, m_plot);

    auto plotData = QSharedPointer<PlotData>::create();

    connect(parser, &PerfParser::summaryDataAvailable,
            [plotData](const Data::Summary data) { plotData->applicationStartTime = data.applicationTime.start; });

    connect(parser, &PerfParser::frequencyDataAvailable, this, [this](const Data::FrequencyResults& results) {
        m_results = results;

        m_page->costSelectionCombobox->clear();
        m_page->costSelectionCombobox->addItem(tr("all"));

        QSet<QString> costs;
        for (const auto& coreData : m_results.cores) {
            for (const auto& costData : coreData.costs) {
                if (!costs.contains(costData.costName)) {
                    costs.insert(costData.costName);
                    m_page->costSelectionCombobox->addItem(costData.costName);
                }
            }
        }
    });

    connect(m_page->costSelectionCombobox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, plotData](int index) {
                m_plot->clearGraphs();
                const auto numCores = m_results.cores.size();
                quint32 core = 0;
                for (const auto& coreData : m_results.cores) {
                    for (const auto& costData : coreData.costs) {
                        // index 0 means all
                        if (index != 0) {
                            if (costData.costName != m_page->costSelectionCombobox->currentText()) {
                                continue;
                            }
                        }

                        auto graph = m_plot->addGraph();
                        graph->setLayer(QStringLiteral("main"));
                        graph->setLineStyle(QCPGraph::lsNone);

                        auto color = QColor::fromHsv(static_cast<int>(255. * (static_cast<float>(core) / numCores)),
                                                     255, 255, 150);
                        graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssSquare, color, color, 4));
                        graph->setAdaptiveSampling(false);
                        graph->setName(QLatin1String("%1 (CPU #%2)").arg(costData.costName, QString::number(core)));
                        graph->addToLegend();
                        graph->setVisible(true);

                        const auto numValues = costData.values.size();
                        QVector<double> times(numValues);
                        QVector<double> costs(numValues);
                        for (int i = 0; i < numValues; ++i) {
                            const auto value = costData.values[i];
                            const auto time = static_cast<double>(value.time - plotData->applicationStartTime);
                            times[i] = time;
                            costs[i] = value.cost;
                        }
                        graph->setData(times, costs, true);
                    }

                    ++core;
                }
                m_plot->xAxis->rescale();
                m_plot->yAxis->rescale();
                m_plot->yAxis->setRangeLower(0.);
                m_plot->replot(QCustomPlot::rpQueuedRefresh);
            });

    m_plot->xAxis->setLabel(tr("Time"));
    m_plot->xAxis->setTicker(QSharedPointer<TimeAxis>(new TimeAxis()));
    m_plot->yAxis->setLabel(tr("Frequency [GHz]"));
    m_plot->legend->setVisible(true);
}

FrequencyPage::~FrequencyPage() = default;

void FrequencyPage::changeEvent(QEvent *event)
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

    for (auto *axis : {m_plot->xAxis, m_plot->yAxis, m_plot->xAxis2, m_plot->yAxis2}) {
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
