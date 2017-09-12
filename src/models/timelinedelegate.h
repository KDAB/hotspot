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
#include <QScopedPointer>

#include "data.h"

class QAbstractItemView;
class QMenu;
class QAction;

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

    void zoom(quint64 timeStart, quint64 timeEnd);

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
};
Q_DECLARE_METATYPE(TimeLineData)

struct FilterAction {
    quint64 startTime = 0;
    quint64 endTime = 0;
    qint32 processId = 0;
    qint32 threadId = 0;
};
Q_DECLARE_TYPEINFO(FilterAction, Q_MOVABLE_TYPE);

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

    void setEventType(int type);

    QMenu* filterMenu() const;

signals:
    // emitted when user wants to filter by time, process id or thread id
    // a zero for any of the values means "show everything"
    void filterRequested(quint64 startTime, quint64 endTime,
                         qint32 processId, qint32 threadId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void filterInByTime(quint64 timeStart, quint64 timeEnd);
    void filterInByProcess(qint32 processId);
    void filterInByThread(qint32 threadId);
    void applyFilter(FilterAction filter);
    void zoomIn(quint64 startTime, quint64 endTime);
    void updateZoomState();
    void resetFilter();
    void filterOut();
    void resetZoom();
    void zoomOut();
    void resetZoomAndFilter();
    void updateFilterActions();

    QAbstractItemView* m_view = nullptr;
    quint64 m_timeSliceStart = 0;
    quint64 m_timeSliceEnd = 0;
    QVector<QPair<quint64, quint64>> m_zoomStack;
    QVector<FilterAction> m_filterStack;
    int m_eventType = 0;
    QScopedPointer<QMenu> m_filterMenu;
    QAction* m_filterOutAction;
    QAction* m_resetFilterAction;
    QAction* m_zoomOutAction;
    QAction* m_resetZoomAction;
    QAction* m_resetZoomAndFilterAction;
};
