/*
  timelinedelegate.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QStyledItemDelegate>
#include <QVector>

#include "data.h"

class QAbstractItemView;

struct TimeLineData
{
    TimeLineData();

    TimeLineData(const Data::Events& events, quint64 maxCost,
                 quint64 minTime, quint64 maxTime,
                 quint64 threadStartTime, quint64 threadEndTime,
                 QRect rect);

    int mapTimeToX(quint64 time) const;

    quint64 mapXToTime(int x) const;

    int mapCostToY(quint64 cost) const;

    void zoom(int xOffset, double scale);

    static const constexpr int padding = 2;
    Data::Events events;
    quint64 maxCost;
    quint64 minTime;
    quint64 maxTime;
    quint64 timeDelta;
    quint64 threadStartTime;
    quint64 threadEndTime;
    int h;
    int w;
    double xMultiplicator;
    double yMultiplicator;
    double zoomOffset;
    double zoomScale;
};
Q_DECLARE_METATYPE(TimeLineData)

class TimeLineDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit TimeLineDelegate(QAbstractItemView* view);
    virtual ~TimeLineDelegate();

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

signals:
    // emitted when user wants to filter by time
    // start == 0 && end == 0 means "show everything"
    void filterByTime(quint64 start, quint64 end);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QAbstractItemView* m_view;
    QPointF m_timeSliceStart;
    QPointF m_timeSliceEnd;
    QVector<QRectF> m_zoomStack;
    QVector<QPair<quint64, quint64>> m_filterStack;
};
