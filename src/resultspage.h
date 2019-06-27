/*
  resultspage.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

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

#include <QWidget>

class QMenu;

namespace Ui {
class ResultsPage;
}

namespace Data {
struct Symbol;
}

class PerfParser;
class ResultsSummaryPage;
class ResultsBottomUpPage;
class ResultsTopDownPage;
class ResultsFlameGraphPage;
class ResultsCallerCalleePage;
class TimeLineDelegate;
class FilterAndZoomStack;

class ResultsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsPage(PerfParser* parser, QWidget* parent = nullptr);
    ~ResultsPage();

    void selectSummaryTab();
    void clear();
    QMenu* filterMenu() const;

public slots:
    void setSysroot(const QString& path);
    void setAppPath(const QString& path);

    void onNavigateToCode(const QString& url, int lineNumber, int columnNumber);
    void onJumpToCallerCallee(const Data::Symbol& symbol);
    void setTimelineVisible(bool visible);

signals:
    void navigateToCode(const QString& url, int lineNumber, int columnNumber);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void repositionFilterBusyIndicator();

    QScopedPointer<Ui::ResultsPage> ui;

    FilterAndZoomStack* m_filterAndZoomStack;
    ResultsSummaryPage* m_resultsSummaryPage;
    ResultsBottomUpPage* m_resultsBottomUpPage;
    ResultsTopDownPage* m_resultsTopDownPage;
    ResultsFlameGraphPage* m_resultsFlameGraphPage;
    ResultsCallerCalleePage* m_resultsCallerCalleePage;
    QMenu* m_filterMenu;
    TimeLineDelegate* m_timeLineDelegate;
    QWidget* m_filterBusyIndicator;
    bool m_timelineVisible;
};
