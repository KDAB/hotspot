/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <atomic>
#include <memory>

namespace Ui {
class TimeLineWidget;
}

namespace Data {
struct Symbol;
}

class PerfParser;
class FilterAndZoomStack;
class TimeLineDelegate;
class TimeAxisHeaderView;

class QMenu;

class TimeLineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimeLineWidget(PerfParser* parser, QMenu* filterMenu, FilterAndZoomStack* filterAndZoomStack,
                            QWidget* parent = nullptr);
    ~TimeLineWidget() override;

    void selectSymbol(const Data::Symbol& symbol);

private:
    std::unique_ptr<Ui::TimeLineWidget> ui;

    PerfParser* m_parser = nullptr;
    FilterAndZoomStack* m_filterAndZoomStack = nullptr;
    TimeLineDelegate* m_timeLineDelegate = nullptr;
    TimeAxisHeaderView* m_timeAxisHeaderView = nullptr;
    std::atomic<uint> m_currentSelectStackJobId;
};
