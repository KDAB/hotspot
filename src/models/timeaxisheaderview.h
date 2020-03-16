#ifndef TIMEAXISHEADERVIEW_H
#define TIMEAXISHEADERVIEW_H

#include <QHeaderView>

#include "data.h"

class FilterAndZoomStack;

class TimeAxisHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit TimeAxisHeaderView(FilterAndZoomStack* filterAndZoomStack, Qt::Orientation orientation, QWidget *parent = nullptr);

public:
    void setTimeRange(const Data::TimeRange &timeRange);
private:
    Data::TimeRange m_timeRange;

private:
    FilterAndZoomStack* m_filterAndZoomStack = nullptr;
public slots:
    void onFilterChanged(const Data::FilterAction &filterAction);
    void onZoomChanged(const Data::ZoomAction &zoomAction);
protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};

class PrefixTickLabels
{
    // Construction
public:
    /**
     * @param min, lower limit of the range
     * @param max, upper limit of the range
     * @param targetNbTicks,
     * @throws std::invalid_argument, min > max
     */
    PrefixTickLabels(double min, double max, unsigned targetNbTicks);
private:
    static const int s_group_10 = 3; // Scientific notation is multiple of 3


    // Prefix
public:
    /** Is there a common value that can be used as prefix?
     */
    bool hasPrefix() const;
    /**
     * @pre hasPrefix()
     */
    double prefixValue() const;
    /** Textual representation of the prefix value
     * @pre hasPrefix()
     * @param placeholder, character used to indicate place of the ticks values
     */
    QString prefixLabel(const QString &placeholder = QLatin1String("xxx")) const;
private:
    int m_prefix_10 = 0;
    double m_prefixValue = 0.0;
    QString m_prefixLabel;


    // Ticks
public:
    using TicksAndLabel = QVector<QPair<double, QString>>;
    const TicksAndLabel &ticksAndLabel() const;
private:
    TicksAndLabel m_ticksAndLabels;


    // Utility
public:
    /** Returns a power of 10 multiple of 1, 2 or 5 for the given value
     * @see "Nice Numbers for Graph Labels", Graphics Gems, Andrew S. Glassner, Elsevier Science, 1990
     */
    static double niceNum(double value);
    /** Prefix for the given power of 10
     * @param power
     * @throws std::invalid_argument, if there is no SI prefix for the given power
     */
    static QString siPrefix(int power);
};

#endif // TIMEAXISHEADERVIEW_H
