/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QSet>

class QTreeView;
class QHeaderView;
class QMenu;

class CostContextMenu : public QObject
{
    Q_OBJECT
public:
    explicit CostContextMenu(QObject* parent = nullptr);
    ~CostContextMenu();

    void addToMenu(QHeaderView* view, QMenu* menu);
    void hideColumns(QTreeView* view);

signals:
    void hiddenColumnsChanged();

private:
    QSet<QString> m_hiddenColumns;
};
