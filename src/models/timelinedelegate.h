/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QScopedPointer>
#include <QSet>
#include <QStyledItemDelegate>

#include "data.h"

class QAbstractItemView;
class QAction;

class FilterAndZoomStack;

struct TimeLineData
{
    TimeLineData();

    TimeLineData(const Data::Events& events, quint64 maxCost, const Data::TimeRange& time,
                 const Data::TimeRange& threadTime, QRect rect);

    int mapTimeToX(quint64 time) const;

    quint64 mapXToTime(int x) const;

    int mapCostToY(quint64 cost) const;

    void zoom(const Data::TimeRange& time);

    template<typename Callback>
    void findSamples(int mappedX, int costType, int lostEventCostId, bool contains,
                     const Data::Events::const_iterator start, const Callback& callback) const;

    static const constexpr int padding = 2;
    Data::Events events;
    quint64 maxCost;
    Data::TimeRange time;
    Data::TimeRange threadTime;
    int h;
    int w;
    double xMultiplicator;
    double yMultiplicator;
};
Q_DECLARE_METATYPE(TimeLineData)

class TimeLineDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit TimeLineDelegate(FilterAndZoomStack* filterAndZoomStack, QAbstractItemView* view);
    virtual ~TimeLineDelegate();

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    void setEventType(int type);
    void setSelectedStacks(const QSet<qint32>& selectedStacks);

signals:
    void stacksHovered(const QSet<qint32>& stacks);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateView();
    void updateZoomState();

    FilterAndZoomStack* m_filterAndZoomStack = nullptr;
    QAbstractItemView* m_view = nullptr;
    Data::TimeRange m_timeSlice;
    QSet<qint32> m_selectedStacks;
    QSet<qint32> m_hoveredStacks;
    int m_eventType = 0;
};
