/*
  frequencypage.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Lieven Hey <lieven.hey@kdab.com>

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
