/*
  treemodel.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include "treemodel.h"

AbstractTreeModel::AbstractTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

AbstractTreeModel::~AbstractTreeModel() = default;

BottomUpModel::BottomUpModel(QObject* parent)
    : TreeModel(parent)
{
}

BottomUpModel::~BottomUpModel() = default;

QString BottomUpModel::headerTitle(Columns column)
{
    switch (column) {
    case Symbol:
        return tr("Symbol");
    case Binary:
        return tr("Binary");
    case Cost:
        return tr("Cost");
    }
    Q_UNREACHABLE();
    return {};
}

QVariant BottomUpModel::displayData(const Data::BottomUp* row, Columns column)
{
    switch (column) {
    case Symbol:
        return row->symbol.symbol;
    case Binary:
        return row->symbol.binary;
    case Cost:
        return row->cost.samples;
    }
    Q_UNREACHABLE();
    return {};
}

TopDownModel::TopDownModel(QObject* parent)
    : TreeModel(parent)
{
}

TopDownModel::~TopDownModel() = default;

QString TopDownModel::headerTitle(Columns column)
{
    switch (column) {
    case Symbol:
        return tr("Symbol");
    case Binary:
        return tr("Binary");
    case InclusiveCost:
        return tr("Inclusive Cost");
    case SelfCost:
        return tr("Self Cost");
    }
    Q_UNREACHABLE();
    return {};
}

QVariant TopDownModel::displayData(const Data::TopDown* row, Columns column)
{
    switch (column) {
    case Symbol:
        return row->symbol.symbol;
    case Binary:
        return row->symbol.binary;
    case InclusiveCost:
        return row->inclusiveCost.samples;
    case SelfCost:
        return row->selfCost.samples;
    }
    Q_UNREACHABLE();
    return {};
}
