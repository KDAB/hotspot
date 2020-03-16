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
protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};

#endif // TIMEAXISHEADERVIEW_H
