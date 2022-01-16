/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "frequencypage.h"

#include <KChart/KChart>
#include <KChart/KChartCartesianAxis>
#include <KChart/KChartPlotter>
#include <KColorScheme>

#include "models/frequencymodel.h"
#include "parsers/perf/perfparser.h"
#include "util.h"

namespace {
class TimeAxis : public KChart::CartesianAxis
{
    Q_OBJECT
public:
    explicit TimeAxis(KChart::AbstractCartesianDiagram* diagram = nullptr)
        : CartesianAxis(diagram)
    {
    }

    const QString customizedLabel(const QString& label) const override
    {
        const auto time = label.toLongLong() - m_applicationStartTime;
        return Util::formatTimeString(time);
    }

    void setApplicationStartTime(const quint64 applicationStartTime)
    {
        m_applicationStartTime = applicationStartTime;
        update();
    }

private:
    quint64 m_applicationStartTime = 0;
};
}

FrequencyPage::FrequencyPage(PerfParser* parser, QWidget* parent)
    : QWidget(parent)
    , m_widget(new KChart::Chart(this))
    , m_model(new FrequencyModel(this))
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_widget);
    setLayout(layout);

    auto* plotter = new KChart::Plotter(this);

    plotter->setModel(m_model);
    m_widget->coordinatePlane()->replaceDiagram(plotter);
    // use available area to 100%
    qobject_cast<KChart::CartesianCoordinatePlane*>(m_widget->coordinatePlane())
        ->setAutoAdjustHorizontalRangeToData(100);

    const auto colorScheme = KColorScheme(QPalette::Active);
    KChart::TextAttributes textAttributes;
    auto fontSize = textAttributes.fontSize();
    fontSize.setAbsoluteValue(font().pointSizeF() - 2);
    textAttributes.setFontSize(fontSize);
    textAttributes.setPen(QPen(colorScheme.foreground().color()));

    auto xAxis = new TimeAxis(plotter);
    xAxis->setTitleText(tr("Time in ns"));
    xAxis->setTextAttributes(textAttributes);
    xAxis->setTitleTextAttributes(textAttributes);

    connect(parser, &PerfParser::summaryDataAvailable,
            [xAxis](const Data::Summary data) { xAxis->setApplicationStartTime(data.applicationTime.start); });

    connect(parser, &PerfParser::frequencyDataAvailable, m_widget, [this, plotter](const Data::FrequencyResults& data) {
        m_model->setResults(data);
        qobject_cast<KChart::CartesianCoordinatePlane*>(m_widget->coordinatePlane())->adjustRangesToData();

        // two colums are one dataset
        for (int i = 0, c = m_model->columnCount() / 2; i < c; i++) {
            auto attr = plotter->dataValueAttributes(i);
            auto mattr = attr.markerAttributes();
            mattr.setMarkerStyle(KChart::MarkerAttributes::Marker4Pixels);
            mattr.setMarkerSize(QSizeF(8.0, 8.0));
            mattr.setVisible(true);
            attr.setMarkerAttributes(mattr);
            auto textAttr = attr.textAttributes();
            textAttr.setVisible(false);
            attr.setTextAttributes(textAttr);
            attr.setVisible(true);
            plotter->setDataValueAttributes(i, attr);
            plotter->setPen(i, Qt::NoPen);
        }
    });

    auto yAxis = new KChart::CartesianAxis(plotter);
    yAxis->setTitleText(tr("Frequency in GHz"));
    yAxis->setTextAttributes(textAttributes);
    yAxis->setTitleTextAttributes(textAttributes);

    xAxis->setPosition(KChart::CartesianAxis::Bottom);
    yAxis->setPosition(KChart::CartesianAxis::Left);
    plotter->addAxis(xAxis);
    plotter->addAxis(yAxis);

    auto legend = new KChart::Legend(plotter, m_widget);
    m_widget->addLegend(legend);
    legend->setTitleText(tr("Legend"));
    legend->setTitleTextAttributes(textAttributes);
    legend->setTextAttributes(textAttributes);
    legend->setPosition(KChart::Position::East);

    m_widget->show();
}

FrequencyPage::~FrequencyPage() = default;

void FrequencyPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        const auto colorScheme = KColorScheme(QPalette::Active);
        KChart::TextAttributes textAttributes;
        textAttributes.setPen(QPen(colorScheme.foreground().color()));

        const auto diagram = qobject_cast<KChart::CartesianCoordinatePlane*>(m_widget->coordinatePlane())->diagram();

        if (!diagram) {
            return;
        }

        const auto axes = qobject_cast<KChart::AbstractCartesianDiagram*>(diagram)->axes();

        for (const auto& axis : axes) {
            axis->setTitleTextAttributes(textAttributes);
            axis->setTextAttributes(textAttributes);
        }

        m_widget->legend()->setTextAttributes(textAttributes);
        m_widget->legend()->setTitleTextAttributes(textAttributes);
    }
}


#include "frequencypage.moc"
