/*
  flamegraph.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#ifndef FLAMEGRAPH_H
#define FLAMEGRAPH_H

#include <QWidget>
#include <QVector>

#include <models/data.h>

class QGraphicsScene;
class QGraphicsView;
class QComboBox;
class QLabel;
class QLineEdit;

class FrameGraphicsItem;

class FlameGraph : public QWidget
{
    Q_OBJECT
public:
    FlameGraph(QWidget* parent = nullptr, Qt::WindowFlags flags = 0);
    ~FlameGraph();

    void setTopDownData(const Data::TopDownResults& topDownData);
    void setBottomUpData(const Data::BottomUpResults& bottomUpData);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void setData(FrameGraphicsItem* rootItem);
    void setSearchValue(const QString& value);
    void navigateBack();
    void navigateForward();

signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);

private:
    void setTooltipItem(const FrameGraphicsItem* item);
    void updateTooltip();
    void showData();
    void selectItem(int item);
    void selectItem(FrameGraphicsItem* item);
    void updateNavigationActions();

    Data::TopDownResults m_topDownData;
    Data::BottomUpResults m_bottomUpData;

    QComboBox* m_costSource;
    QGraphicsScene* m_scene;
    QGraphicsView* m_view;
    QLabel* m_displayLabel;
    QLabel* m_searchResultsLabel;
    QLineEdit* m_searchInput = nullptr;
    QAction* m_forwardAction = nullptr;
    QAction* m_backAction = nullptr;
    QAction* m_resetAction = nullptr;
    const FrameGraphicsItem* m_tooltipItem = nullptr;
    FrameGraphicsItem* m_rootItem = nullptr;
    QVector<FrameGraphicsItem*> m_selectionHistory;
    int m_selectedItem = -1;
    int m_minRootWidth = 0;
    bool m_showBottomUpData = false;
    bool m_collapseRecursion = false;
    bool m_buildingScene = false;
    // cost threshold in percent, items below that value will not be shown
    double m_costThreshold = 0.1;
};

#endif // FLAMEGRAPH_H
