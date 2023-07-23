/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>

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

    std::unique_ptr<Ui::ResultsPage> ui;
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
