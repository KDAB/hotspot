/*
  timeaxisheaderview.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2020 Koen Poppe
  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

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

#include <QHeaderView>

#include "data.h"

class FilterAndZoomStack;

class TimeAxisHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit TimeAxisHeaderView(const FilterAndZoomStack* filterAndZoomStack, QWidget* parent = nullptr);
    static const int s_tickHeight = 4;

public:
    void setTimeRange(const Data::TimeRange& timeRange);

protected slots:
    void emitHeaderDataChanged();

private:
    Data::TimeRange m_timeRange;

private:
    const FilterAndZoomStack* m_filterAndZoomStack = nullptr;

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
};
