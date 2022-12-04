/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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

void CostContextMenu::addToMenu(QHeaderView* view, QMenu* menu)
{
    for (int i = 1; i < view->count(); ++i) {
        const auto name = view->model()->headerData(i, Qt::Horizontal).toString();
        auto* action = menu->addAction(name);
        action->setCheckable(true);
        action->setChecked(!view->isSectionHidden(i));
        connect(action, &QAction::toggled, this, [this, view, name, i](bool visible) {
            view->setSectionHidden(i, !visible);
            if (visible) {
                m_hiddenColumns.remove(name);
            } else {
                m_hiddenColumns.insert(name);
            }

            emit hiddenColumnsChanged();
        });
    }
}

void CostContextMenu::hideColumns(QTreeView* view)
{
    const auto* model = view->model();
    for (int i = 1, size = model->columnCount(); i < size; i++) {
        const auto name = model->headerData(i, Qt::Orientation::Horizontal).toString();
        view->setColumnHidden(i, m_hiddenColumns.contains(name));
    }
}
