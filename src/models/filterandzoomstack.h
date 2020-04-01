/*
  filterandzoomstack.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2019-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QObject>

#include "data.h"

class QAction;

class FilterAndZoomStack : public QObject
{
    Q_OBJECT
public:
    explicit FilterAndZoomStack(QObject* parent = nullptr);
    ~FilterAndZoomStack();

    Data::FilterAction filter() const;
    Data::ZoomAction zoom() const;

    struct Actions
    {
        QAction* filterOut = nullptr;
        QAction* resetFilter = nullptr;
        QAction* zoomOut = nullptr;
        QAction* resetZoom = nullptr;
        QAction* resetFilterAndZoom = nullptr;
        QAction* filterInBySymbol = nullptr;
        QAction* filterOutBySymbol = nullptr;
    };

    Actions actions() const;

    void clear();

public slots:
    void filterInByTime(const Data::TimeRange &time);
    void filterInByProcess(qint32 processId);
    void filterOutByProcess(qint32 processId);
    void filterInByThread(qint32 threadId);
    void filterOutByThread(qint32 threadId);
    void filterInByCpu(quint32 cpuId);
    void filterOutByCpu(quint32 cpuId);
    void filterInBySymbol(const Data::Symbol &symbol);
    void filterOutBySymbol(const Data::Symbol &symbol);
    void applyFilter(Data::FilterAction filter);
    void resetFilter();
    void filterOut();
    void zoomIn(const Data::TimeRange &time);
    void resetZoom();
    void zoomOut();
    void resetFilterAndZoom();

signals:
    void filterChanged(const Data::FilterAction& filter);
    void zoomChanged(const Data::ZoomAction& zoom);

private:
    void updateActions();

    Actions m_actions;
    QVector<Data::FilterAction> m_filterStack;
    QVector<Data::ZoomAction> m_zoomStack;
};
