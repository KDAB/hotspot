/*
  resultspage.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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
class QAction;

namespace Ui {
class ResultsPage;
}

namespace Data {
struct Symbol;
}

namespace KDDockWidgets {
class MainWindow;
class DockWidget;
class DockWidgetBase;
}

class PerfParser;
class ResultsSummaryPage;
class ResultsBottomUpPage;
class ResultsTopDownPage;
class ResultsFlameGraphPage;
class ResultsCallerCalleePage;
class ResultsDisassemblyPage;
class FilterAndZoomStack;
class TimeLineWidget;
class CostContextMenu;
class FrequencyPage;

class ResultsPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsPage(PerfParser* parser, QWidget* parent = nullptr);
    ~ResultsPage();

    void selectSummaryTab();
    void clear();
    QMenu* filterMenu() const;
    QMenu* exportMenu() const;
    QList<QAction*> windowActions() const;

    void initDockWidgets(const QVector<KDDockWidgets::DockWidgetBase*>& restored);

public slots:
    void setSysroot(const QString& path);
    void setAppPath(const QString& path);
    void setObjdump(const QString& objdump);

    void onJumpToCallerCallee(const Data::Symbol& symbol);
    void onOpenEditor(const Data::Symbol& symbol);
    void showError(const QString& message);
    void onJumpToDisassembly(const Data::Symbol& symbol);

signals:
    void navigateToCode(const QString& url, int lineNumber, int columnNumber);

private:
    void resizeEvent(QResizeEvent* event) override;
    void repositionFilterBusyIndicator();

    QScopedPointer<Ui::ResultsPage> ui;
    KDDockWidgets::MainWindow* m_contents;
    FilterAndZoomStack* m_filterAndZoomStack;
    CostContextMenu* m_costContextMenu;
    QMenu* m_filterMenu;
    QMenu* m_exportMenu;
    KDDockWidgets::DockWidget* m_summaryPageDock;
    ResultsSummaryPage* m_resultsSummaryPage;
    KDDockWidgets::DockWidget* m_bottomUpDock;
    ResultsBottomUpPage* m_resultsBottomUpPage;
    KDDockWidgets::DockWidget* m_topDownDock;
    ResultsTopDownPage* m_resultsTopDownPage;
    KDDockWidgets::DockWidget* m_flameGraphDock;
    ResultsFlameGraphPage* m_resultsFlameGraphPage;
    KDDockWidgets::DockWidget* m_callerCalleeDock;
    ResultsCallerCalleePage* m_resultsCallerCalleePage;
    KDDockWidgets::DockWidget* m_disassemblyDock;
    ResultsDisassemblyPage* m_resultsDisassemblyPage;
    KDDockWidgets::DockWidget* m_timeLineDock;
    TimeLineWidget* m_timeLineWidget;
    FrequencyPage* m_frequencyPage = nullptr;
    KDDockWidgets::DockWidget* m_frequencyDock = nullptr;
    QWidget* m_filterBusyIndicator = nullptr;
    bool m_timelineVisible;
};
