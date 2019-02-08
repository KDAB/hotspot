/*
  timelinedelegate.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

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

#pragma once

#include <QScopedPointer>
#include <QStyledItemDelegate>
#include <QVector>

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

    void zoom(const Data::TimeRange &time);

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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateView();
    void updateZoomState();

    FilterAndZoomStack* m_filterAndZoomStack = nullptr;
    QAbstractItemView* m_view = nullptr;
    Data::TimeRange m_timeSlice;
    int m_eventType = 0;
};
