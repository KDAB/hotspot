#ifndef TIMEAXISHEADERVIEW_H
#define TIMEAXISHEADERVIEW_H

#include <QHeaderView>

#include "data.h"

class FilterAndZoomStack;

class TimeAxisHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit TimeAxisHeaderView(FilterAndZoomStack* filterAndZoomStack, QWidget* parent = nullptr);
    static const int s_tickHeight = 4;

public:
    void setTimeRange(const Data::TimeRange& timeRange);

protected slots:
    void emitHeaderDataChanged();

private:
    Data::TimeRange m_timeRange;

private:
    FilterAndZoomStack* m_filterAndZoomStack = nullptr;

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
};

#endif // TIMEAXISHEADERVIEW_H
