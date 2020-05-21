#include "timeaxisheaderview.h"

#include <QDebug>
#include <QPainter>
#include <QtMath>

#include "eventmodel.h"
#include "filterandzoomstack.h"
#include "../util.h"

#include <PrefixTickLabels.h>

TimeAxisHeaderView::TimeAxisHeaderView(const FilterAndZoomStack* filterAndZoomStack, QWidget* parent)
    : QHeaderView(Qt::Horizontal, parent)
    , m_filterAndZoomStack(filterAndZoomStack)
{
    setMinimumHeight(3*fontMetrics().height() + s_tickHeight);
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

    const double resolution = (end - start) / rect.width();
    const auto xForTime = [rect, start, resolution](const double time) {
        return rect.x() + static_cast<int>(std::round((time - start) / resolution));
    };

    const int fontSize = painter->fontMetrics().height();
    const int startY = rect.height() - s_tickHeight - 2*fontSize;
    // Width of a tick label that is prefixed, this is at most 4 digits plus an SI prefix.
    // This includes a minus sign for ticks to the left of the prefix value
    const int maxPrefixedLabelWidth = painter->fontMetrics().horizontalAdvance(QStringLiteral("-xXXXm"));
    const int targetNbTicks = rect.width() / maxPrefixedLabelWidth;
    const PrefixTickLabels pfl(start, end, targetNbTicks);

    const QColor tickColor = palette().windowText().color();
    const QColor prefixedColor = palette().highlight().color();

    // Draw the long prefix tick and its label
    painter->setPen(tickColor);
    if (pfl.hasPrefix()) {
        const auto placeholder = QStringLiteral("xxx");
        const int prefixWidth = painter->fontMetrics().horizontalAdvance(pfl.prefixLabel(placeholder));
        const int prefixCenter = xForTime(pfl.prefixValue());

        QRect placeHolderRect(prefixCenter - prefixWidth / 2, startY, prefixWidth, fontSize);
        if (placeHolderRect.x() < rect.x())
            placeHolderRect.translate(rect.x() - placeHolderRect.x(), 0);

        QRect bounding;
        painter->drawText(placeHolderRect, Qt::AlignBottom | Qt::AlignLeft, pfl.prefixLabel({}), &bounding);
        painter->setPen(prefixedColor);
        bounding.translate(bounding.width(), 0);
        bounding.setWidth(prefixWidth);
        painter->drawText(bounding, Qt::AlignBottom | Qt::AlignLeft, placeholder);
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
