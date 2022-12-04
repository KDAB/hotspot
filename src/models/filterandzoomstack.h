/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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
        QAction* filterInByBinary = nullptr;
        QAction* filterOutByBinary = nullptr;
    };

    Actions actions() const;

    void clear();

public slots:
    void filterInByTime(Data::TimeRange time);
    void filterInByProcess(qint32 processId);
    void filterOutByProcess(qint32 processId);
    void filterInByThread(qint32 threadId);
    void filterOutByThread(qint32 threadId);
    void filterInByCpu(quint32 cpuId);
    void filterOutByCpu(quint32 cpuId);
    void filterInBySymbol(const Data::Symbol& symbol);
    void filterOutBySymbol(const Data::Symbol& symbol);
    void filterInByBinary(const QString& binary);
    void filterOutByBinary(const QString& binary);
    void applyFilter(Data::FilterAction filter);
    void resetFilter();
    void filterOut();
    void zoomIn(Data::TimeRange time);
    void resetZoom();
    void zoomOut();
    void resetFilterAndZoom();

signals:
    void filterChanged(const Data::FilterAction& filter);
    void zoomChanged(Data::ZoomAction zoom);

private:
    void updateActions();

    Actions m_actions;
    QVector<Data::FilterAction> m_filterStack;
    QVector<Data::ZoomAction> m_zoomStack;
};
