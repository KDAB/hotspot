#include "timeaxisheaderview.h"

#include <QPainter>
#include <QtMath>
#include <QDebug>

#include "filterandzoomstack.h"

TimeAxisHeaderView::TimeAxisHeaderView(FilterAndZoomStack *filterAndZoomStack, Qt::Orientation orientation, QWidget *parent)
    : QHeaderView(orientation, parent)
    , m_filterAndZoomStack(filterAndZoomStack)
{
    setMinimumHeight(40);

    auto emitHeaderDataChanged = [this]() {
        emit headerDataChanged(this->orientation(), 1, 1);
    };
    connect(filterAndZoomStack, &FilterAndZoomStack::filterChanged, this, emitHeaderDataChanged);
    connect(filterAndZoomStack, &FilterAndZoomStack::zoomChanged, this, emitHeaderDataChanged);
}

void TimeAxisHeaderView::setTimeRange(const Data::TimeRange &timeRange)
{
    m_timeRange = timeRange;
    emit headerDataChanged(orientation(), 1, 1);
}

void TimeAxisHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (painter == nullptr) return;
    if (logicalIndex != 1) return; // TODO: role based?

    auto zoomTime = m_filterAndZoomStack->zoom().time;
    if (!zoomTime.isValid())
    {
        zoomTime = m_timeRange; // full
    }
    if (!zoomTime.isValid())
    {
        return; // bailing out, no valid range to show
    }
    const double oneNanoSecond = 1.0e-9;
    const double start = zoomTime.start * oneNanoSecond;
    const double end = zoomTime.end * oneNanoSecond;

    QBrush text(QColor(0,0,0));
    painter->setBrush(text);

    const double resolution = (end - start) / rect.width();
    const auto xForTime = [rect, start, resolution](const double time) {
        return rect.x() + qRound((time - start)/resolution);
    };

    const int fontSize = painter->fontMetrics().height();
    const int endLabelWidth = painter->fontMetrics().horizontalAdvance(QLatin1String("-xXXXm"));
    const int targetNbTicks = rect.width() / endLabelWidth;

    qDebug() << __FUNCTION__ << start << end << targetNbTicks;
    const PrefixTickLabels pfl(start, end, targetNbTicks);

    painter->setPen(QColor(0,0,0));
    if (pfl.hasPrefix())
    {
        const QString placeholder = QLatin1String("xxx");
        const int prefixWidth = painter->fontMetrics().horizontalAdvance(pfl.prefixLabel(placeholder));
        const int prefixCenter = xForTime(pfl.prefixValue());

        QRect placeHolderRect(prefixCenter - prefixWidth/2, rect.y(), prefixWidth, fontSize);
        if (placeHolderRect.x() < rect.x())
        {
            placeHolderRect.translate(rect.x() - placeHolderRect.x(), 0);
        }

        QRect bounding;
        painter->drawText(placeHolderRect, Qt::AlignBottom | Qt::AlignLeft, pfl.prefixLabel({}), &bounding);
        painter->setPen(QColor(0,0,255));
        bounding.translate(bounding.width(), 0);
        bounding.setWidth(prefixWidth);
        painter->drawText(bounding, Qt::AlignBottom | Qt::AlignLeft, placeholder);
    }

    for (const auto &tickAndLabel : pfl.ticksAndLabel())
    {
        const auto x = xForTime(tickAndLabel.first);
        if (std::abs(tickAndLabel.first - pfl.prefixValue()) < oneNanoSecond)
        {
            painter->setPen(QColor(0,0,0));
            painter->drawLine(x, rect.y() + fontSize, x, rect.y() + rect.height());
        }
        else
        {
            painter->setPen(QColor(0,0,255));
            QRect labelRect(x - endLabelWidth/2, rect.y() + fontSize, endLabelWidth, fontSize);
            painter->drawText(labelRect, Qt::AlignCenter | Qt::AlignBottom, tickAndLabel.second);
            painter->drawLine(x, labelRect.y() + fontSize, x, labelRect.y() + rect.height());
        }
    }
}
    

// Construction

PrefixTickLabels::PrefixTickLabels(const double min, const double max, const unsigned targetNbTicks)
{
    const double range = std::abs(max - min);
    const double mid = (min + max)/2;

    const auto range_10 = static_cast<int>(floor(log10(range))) - (log10(range) <= 0.0 ? 1 : 0);

    const int scale_10 = s_group_10 * static_cast<int>((range_10 - (range_10 < 0 ? s_group_10 - 1 : 0 ))/ s_group_10);
    const double inv_scale = pow(10, -scale_10);

    m_prefix_10 = scale_10 + s_group_10;
    const double prefix_power = pow(10, m_prefix_10);

    // Prefer the left most prefix in range
    std::vector<double> prefixCandidates {
        ceil(min / prefix_power) * prefix_power,
        floor(mid / prefix_power) * prefix_power,
        floor(max / prefix_power) * prefix_power,
        };
    std::sort(prefixCandidates.begin(), prefixCandidates.end());
    const auto prefixCandidate = std::find_if(prefixCandidates.begin(), prefixCandidates.end(), [min, max](const double candidate) {
        return min <= candidate && candidate <= max;
    });
    if (prefixCandidate != prefixCandidates.end())
    {
        m_prefixValue = *prefixCandidate;
    }
    else
    {
        // .. if none found, take the left most
        m_prefixValue = prefixCandidates.front();
    }

    const double spacing = niceNum(range / targetNbTicks);
    const int labelFraction_10 = std::max(0, scale_10 - static_cast<int>(floor(log10(spacing))));

    const double niceMin = ceil(min / spacing) * spacing;
    const int nbTicks = static_cast<int>(floor((max - niceMin) / spacing));

    const auto siPrefix = PrefixTickLabels::siPrefix(scale_10);
    m_ticksAndLabels.reserve(nbTicks);
    for (int j = 0; j <= nbTicks; j++)
    {
        const double tick = niceMin + j * spacing;
        const auto tickLabel = QString(QLatin1String("%1%2")).arg((tick - m_prefixValue) * inv_scale, 0, 'f', labelFraction_10).arg(tick != 0.0 ? siPrefix : QString());
        m_ticksAndLabels.push_back({ tick, tickLabel });
    }
}


// Prefix

bool PrefixTickLabels::hasPrefix() const
{
    return m_prefixValue != 0.0;
}

double PrefixTickLabels::prefixValue() const
{
    return m_prefixValue;
}

QString PrefixTickLabels::prefixLabel(const QString &placeholder) const
{
    if (!hasPrefix())
    {
        return {};
    }

    if (m_prefix_10 > 1)
    {
        auto prefix = QString::number(static_cast<int>(floor(m_prefixValue/pow(10, s_group_10))));
        return prefix + placeholder;
    }
    if (m_prefix_10 == 0)
    {
        auto prefix = QString::number(static_cast<int>(floor(m_prefixValue)));
        return prefix + QLatin1Char('.') + placeholder;
    }

    auto prefix = QString(QLatin1String("%1")).arg(m_prefixValue, 0, 'f', -m_prefix_10);
    return prefix + placeholder;
}


// Ticks

const PrefixTickLabels::TicksAndLabel &PrefixTickLabels::ticksAndLabel() const
{
    return m_ticksAndLabels;
}


// Utility

double PrefixTickLabels::niceNum(double value)
{
    const auto value_10 = static_cast<int>(floor(log10(value)));
    const auto fraction = value * pow(10, -value_10);

    int niceFraction = 10;
    if (fraction <= 1.0)
    {
        niceFraction = 1;
    }
    else if (fraction <= 2.0)
    {
        niceFraction = 2;
    }
    else if (fraction <= 5.0)
    {
        niceFraction = 5;
    }

    return niceFraction * pow(10, value_10);
}

QString PrefixTickLabels::siPrefix(int power)
{
    switch(power)
    {
    case 24: return QStringLiteral("Y"); //yotta
    case 21: return QStringLiteral("Z"); // zetta
    case 18: return QStringLiteral("E"); // exa
    case 15: return QStringLiteral("P"); // peta
    case 12: return QStringLiteral("T"); // tera
    case 9: return QStringLiteral("G"); // giga
    case 6: return QStringLiteral("M"); // mega
    case 3: return QStringLiteral("k"); // kilo
    case 2: return QStringLiteral("h"); // hecto
    case 1: return QStringLiteral("da"); // deca
    case 0: return {};
    case -1: return QStringLiteral("d"); // deci
    case -2: return QStringLiteral("c"); // centi
    case -3: return QStringLiteral("m"); // milli
    case -6: return QStringLiteral("µ"); // micro
    case -9: return QStringLiteral("n"); // nano
    case -12: return QStringLiteral("p"); // pico
    case -15: return QStringLiteral("f"); // femto
    case -18: return QStringLiteral("a"); // atto
    case -21: return QStringLiteral("z"); // zepto
    case -24: return QStringLiteral("y"); // tocto
    }
    return QLatin1String("?");
}

