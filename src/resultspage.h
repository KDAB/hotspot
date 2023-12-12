/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>

#include "dockwidgets.h"

class QMenu;
class QAction;

namespace Ui {
class ResultsPage;
}

namespace Data {
struct Symbol;
struct FileLine;
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

    void initDockWidgets(const QVector<CoreDockWidget*>& restored);

public slots:
    void setSysroot(const QString& path);
    void setAppPath(const QString& path);
    void setObjdump(const QString& objdump);

    void onJumpToCallerCallee(const Data::Symbol& symbol);
    void onOpenEditor(const Data::Symbol& symbol);
    void showError(const QString& message);
    void onJumpToDisassembly(const Data::Symbol& symbol);
    void onJumpToSourceCode(const Data::Symbol& symbol, const Data::FileLine& line);

signals:
    void navigateToCode(const QString& url, int lineNumber, int columnNumber);

private:
    void resizeEvent(QResizeEvent* event) override;
    void repositionFilterBusyIndicator();

    std::unique_ptr<Ui::ResultsPage> ui;
    DockMainWindow* m_contents;
    FilterAndZoomStack* m_filterAndZoomStack;
    CostContextMenu* m_costContextMenu;
    QMenu* m_filterMenu;
    QMenu* m_exportMenu;
    DockWidget* m_summaryPageDock;
    ResultsSummaryPage* m_resultsSummaryPage;
    DockWidget* m_bottomUpDock;
    ResultsBottomUpPage* m_resultsBottomUpPage;
    DockWidget* m_topDownDock;
    ResultsTopDownPage* m_resultsTopDownPage;
    DockWidget* m_flameGraphDock;
    ResultsFlameGraphPage* m_resultsFlameGraphPage;
    DockWidget* m_callerCalleeDock;
    ResultsCallerCalleePage* m_resultsCallerCalleePage;
    DockWidget* m_disassemblyDock;
    ResultsDisassemblyPage* m_resultsDisassemblyPage;
    DockWidget* m_timeLineDock;
    TimeLineWidget* m_timeLineWidget;
    FrequencyPage* m_frequencyPage = nullptr;
    DockWidget* m_frequencyDock = nullptr;
    QWidget* m_filterBusyIndicator = nullptr;
    bool m_timelineVisible;
};
