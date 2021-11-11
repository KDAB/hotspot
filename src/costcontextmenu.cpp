/*
  costcontextmenu.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Lieven Hey <lieven.hey@kdab.com>

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

#include "costcontextmenu.h"

#include <QHeaderView>
#include <QMenu>
#include <QTreeView>

CostContextMenu::CostContextMenu(QObject* parent)
    : QObject(parent)
{
}

CostContextMenu::~CostContextMenu() = default;

void CostContextMenu::addToMenu(QHeaderView *view, QMenu *menu)
{
    for (int i = 1; i < view->count(); ++i) {
        const auto name = view->model()->headerData(i, Qt::Horizontal).toString();
        auto* action = menu->addAction(name);
        action->setCheckable(true);
        action->setChecked(!view->isSectionHidden(i));
        connect(action, &QAction::toggled, this, [this, view, name, i](bool visible) { view->setSectionHidden(i, !visible);
            if (visible) {
                m_hiddenColumns.remove(name);
            } else {
                m_hiddenColumns.insert(name);
            }

            emit hiddenColumnsChanged();
        });
    }
}

void CostContextMenu::hideColumns(QTreeView *view)
{
    const auto model = view->model();
    for (int i = 1, size = model->columnCount(); i < size; i++) {
        const auto name = model->headerData(i, Qt::Orientation::Horizontal).toString();
        view->setColumnHidden(i, m_hiddenColumns.contains(name));
    }
}
