/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QVector>
#include <QWidget>

#include <models/data.h>

class QGraphicsScene;
class QGraphicsView;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class KSqueezedTextLabel;

class FrameGraphicsItem;
class FilterAndZoomStack;

class FlameGraph : public QWidget
{
    Q_OBJECT
public:
    explicit FlameGraph(QWidget* parent = nullptr, Qt::WindowFlags flags = {});
    ~FlameGraph();

    void setHoveredStacks(const QVector<QVector<Data::Symbol>>& stacks);
    void setFilterStack(FilterAndZoomStack* filterStack);
    void setTopDownData(const Data::TopDownResults& topDownData);
    void setBottomUpData(const Data::BottomUpResults& bottomUpData);
    void clear();

    QImage toImage() const;
    void saveSvg(const QString& fileName) const;

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void setData(FrameGraphicsItem* rootItem);
    void setSearchValue(const QString& value);
    void navigateBack();
    void navigateForward();

signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);
    void openEditor(const Data::Symbol& symbol);
    void selectSymbol(const Data::Symbol& symbol);
    void selectStack(const QVector<Data::Symbol>& stack);
    void jumpToDisassembly(const Data::Symbol& symbol);
    void uiResetRequested();

private:
    void setTooltipItem(const FrameGraphicsItem* item);
    void updateTooltip();
    void showData();
    void selectItem(int item);
    void selectItem(FrameGraphicsItem* item);
    void updateNavigationActions();
    void rebuild();

    Data::TopDownResults m_topDownData;
    Data::BottomUpResults m_bottomUpData;

    FilterAndZoomStack* m_filterStack = nullptr;
    QComboBox* m_costSource;
    QGraphicsScene* m_scene;
    QGraphicsView* m_view;
    KSqueezedTextLabel* m_displayLabel;
    QLabel* m_searchResultsLabel;
    QLineEdit* m_searchInput = nullptr;
    QAction* m_forwardAction = nullptr;
    QAction* m_backAction = nullptr;
    QAction* m_resetAction = nullptr;
    QPushButton* m_backButton = nullptr;
    QPushButton* m_forwardButton = nullptr;
    QLabel* m_colorSchemeLabel = nullptr;
    QComboBox* m_colorSchemeSelector = nullptr;
    const FrameGraphicsItem* m_tooltipItem = nullptr;
    FrameGraphicsItem* m_rootItem = nullptr;
    QVector<FrameGraphicsItem*> m_selectionHistory;
    int m_selectedItem = -1;
    int m_minRootWidth = 0;
    bool m_showBottomUpData = false;
    bool m_collapseRecursion = false;
    bool m_buildingScene = false;
    // cost threshold in percent, items below that value will not be shown
    static const constexpr double DEFAULT_COST_THRESHOLD = 0.1;
    double m_costThreshold = DEFAULT_COST_THRESHOLD;
    QVector<QVector<Data::Symbol>> m_hoveredStacks;
};
