/*
    SPDX-FileCopyrightText: Koen Poppe
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "timeaxisheaderview.h"

#include <QDebug>
#include <QPainter>
#include <QtMath>

#include <KColorScheme>

#include "../util.h"
#include "eventmodel.h"
#include "filterandzoomstack.h"

#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>

#include <PrefixTickLabels.h>

namespace {
auto xForTimeFactory(const Data::TimeRange& timeRange, const Data::TimeRange& zoomTime, int width, int pos)
{
    const double oneNanoSecond = 1.0e-9;
    const double start = (zoomTime.start - timeRange.start) * oneNanoSecond;
    const double end = (zoomTime.end - timeRange.start) * oneNanoSecond;

    const double resolution = (end - start) / width;

    return [pos, start, resolution](double time) {
        return pos + static_cast<int>(std::round((time - start) / resolution));
    };
}
}

TimeAxisHeaderView::TimeAxisHeaderView(const FilterAndZoomStack* filterAndZoomStack, QWidget* parent)
    : QHeaderView(Qt::Horizontal, parent)
    , m_filterAndZoomStack(filterAndZoomStack)
{
    setMinimumHeight(3 * fontMetrics().height() + s_tickHeight);
    setStretchLastSection(true);

    connect(filterAndZoomStack, &FilterAndZoomStack::filterChanged, this, &TimeAxisHeaderView::emitHeaderDataChanged);
    connect(filterAndZoomStack, &FilterAndZoomStack::zoomChanged, this, &TimeAxisHeaderView::emitHeaderDataChanged);
}

void TimeAxisHeaderView::setTimeRange(const Data::TimeRange& timeRange)
{
    m_timeRange = timeRange;
    emitHeaderDataChanged();
}

void TimeAxisHeaderView::emitHeaderDataChanged()
{
    emit headerDataChanged(this->orientation(), EventModel::EventsColumn, EventModel::EventsColumn);
}

bool TimeAxisHeaderView::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        auto helpEvent = static_cast<QHelpEvent*>(event);

        auto zoomTime = m_filterAndZoomStack->zoom().time;
        if (!zoomTime.isValid())
            zoomTime = m_timeRange; // full

        const auto xForTime = xForTimeFactory(m_timeRange, zoomTime, sectionSize(EventModel::EventsColumn),
                                              sectionPosition(EventModel::EventsColumn));

        const auto oneNanoSecond = 1e-9;
        for (const auto& tracepoint : qAsConst(m_tracepoints.tracepoints)) {
            if (zoomTime.contains(tracepoint.time)) {
                if (helpEvent->pos().x() == xForTime((tracepoint.time - m_timeRange.start) * oneNanoSecond)) {
                    QToolTip::showText(helpEvent->globalPos(), tracepoint.name);
                    return true;
                }
            }
        }

        QToolTip::hideText();
        event->ignore();

        return true;
    }
    return QWidget::event(event);
}

void TimeAxisHeaderView::setTracepoints(const Data::TracepointResults& tracepoints)
{
    m_tracepoints = tracepoints;
    update();
}

void TimeAxisHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
{
    if (painter == nullptr)
        return;

    // Draw the default header view (background, title, sort indicator)
    painter->save();
    QHeaderView::paintSection(painter, rect, logicalIndex);
    painter->restore();
    if (logicalIndex != EventModel::EventsColumn)
        return;

    // Setup the tick labels
    auto zoomTime = m_filterAndZoomStack->zoom().time;
    if (!zoomTime.isValid())
        zoomTime = m_timeRange; // full
    if (!zoomTime.isValid())
        return; // bailing out, no valid range to show

    const double oneNanoSecond = 1.0e-9;
    const double start = (zoomTime.start - m_timeRange.start) * oneNanoSecond;
    const double end = (zoomTime.end - m_timeRange.start) * oneNanoSecond;

    const auto xForTime = xForTimeFactory(m_timeRange, zoomTime, sectionSize(EventModel::EventsColumn),
                                          sectionPosition(EventModel::EventsColumn));

    const int fontSize = painter->fontMetrics().height();
    const int startY = rect.height() - s_tickHeight - 2 * fontSize;
    // Width of a tick label that is prefixed, this is at most 4 digits plus an SI prefix.
    // This includes a minus sign for ticks to the left of the prefix value
    const int maxPrefixedLabelWidth = painter->fontMetrics().horizontalAdvance(QStringLiteral("-xXXXms"));
    const int targetNbTicks = rect.width() / maxPrefixedLabelWidth;
    const PrefixTickLabels pfl(start, end, targetNbTicks, QStringLiteral("s"));

    const QColor tickColor = palette().windowText().color();
    const QColor prefixedColor = palette().highlight().color();

    if (!m_tracepoints.tracepoints.isEmpty()) {
        KColorScheme scheme(palette().currentColorGroup());
        QPen tracepointPen(scheme.foreground(KColorScheme::LinkText), 1);
        painter->setPen(tracepointPen);

        for (const auto& tracepoint : m_tracepoints.tracepoints) {
            if (!zoomTime.contains(tracepoint.time))
                continue;
            const auto x = xForTime((tracepoint.time - m_timeRange.start) * oneNanoSecond);
            painter->drawLine(x, rect.height() / 2, x, rect.height());
        }
    }

    // Draw the long prefix tick and its label

    if (pfl.hasPrefix()) {
        const auto placeholder = QStringLiteral("xxx");
        const int prefixWidth = painter->fontMetrics().horizontalAdvance(pfl.prefixLabel(placeholder));
        const int prefixCenter = xForTime(pfl.prefixValue());

        QRect placeHolderRect(prefixCenter - prefixWidth / 2, startY, prefixWidth, fontSize);
        if (placeHolderRect.x() < rect.x())
            placeHolderRect.translate(rect.x() - placeHolderRect.x(), 0);

        // Leading
        QRect bounding;
        painter->setPen(tickColor);
        painter->drawText(placeHolderRect, Qt::AlignBottom | Qt::AlignLeft, pfl.prefixLabelLeading(), &bounding);
        placeHolderRect.translate(bounding.width(), 0);

        // Placeholder
        painter->setPen(prefixedColor);
        painter->drawText(placeHolderRect, Qt::AlignBottom | Qt::AlignLeft, placeholder, &bounding);
        placeHolderRect.translate(bounding.width(), 0);

        // Trailing
        painter->setPen(tickColor);
        painter->drawText(placeHolderRect, Qt::AlignBottom | Qt::AlignLeft, pfl.prefixLabelTrailing(), &bounding);

        painter->setPen(prefixedColor);
    } else {
        painter->setPen(tickColor);
    }

    // Draw the regular ticks and their labels
    for (const auto& tickAndLabel : pfl.ticksAndLabel()) {
        const auto x = xForTime(tickAndLabel.first);
        if (pfl.hasPrefix() && std::abs(tickAndLabel.first - pfl.prefixValue()) < oneNanoSecond) {
            painter->setPen(tickColor);
            painter->drawLine(x, startY + fontSize, x, rect.y() + rect.height());
            painter->setPen(prefixedColor);
        } else {
            // Keep text within the header
            Qt::Alignment hAlignment = Qt::AlignCenter;
            QRect labelRect(x - maxPrefixedLabelWidth / 2, startY + fontSize, maxPrefixedLabelWidth, fontSize);
            if (labelRect.x() < rect.x()) {
                labelRect.translate(rect.x() - labelRect.x(), 0);
                hAlignment = Qt::AlignLeft;
            }
            if (labelRect.right() > rect.right()) {
                labelRect.translate(rect.right() - labelRect.right(), 0);
                hAlignment = Qt::AlignRight;
            }
            painter->drawText(labelRect, hAlignment | Qt::AlignBottom, tickAndLabel.second);
            painter->drawLine(x, labelRect.y() + fontSize, x, labelRect.y() + rect.height());
        }
    }
}
