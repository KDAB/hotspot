/*
    SPDX-FileCopyrightText: Koen Poppe
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QHeaderView>

#include "data.h"

class FilterAndZoomStack;
class QEvent;

class TimeAxisHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit TimeAxisHeaderView(const FilterAndZoomStack* filterAndZoomStack, QWidget* parent = nullptr);
    static const int s_tickHeight = 4;

public:
    void setTimeRange(Data::TimeRange timeRange);

protected slots:
    void emitHeaderDataChanged();

private:
    Data::TimeRange m_timeRange;
    const FilterAndZoomStack* m_filterAndZoomStack = nullptr;

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const override;
};
