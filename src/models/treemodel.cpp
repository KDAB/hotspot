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
#include "../util.h"

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

QString BottomUpModel::headerToolTip(Columns column)
{
    switch (column) {
    case Symbol:
        return tr("The symbol's function name. May be empty when debug information is missing.");
    case Binary:
        return tr("The name of the executable the symbol resides in. May be empty when debug information is missing.");
    case Cost:
        return tr("The symbol's inclusive cost, i.e. the number of samples attributed to this symbol, both directly and indirectly.");
    }
    Q_UNREACHABLE();
    return {};
}

QVariant BottomUpModel::displayData(const Data::BottomUp* row, Columns column)
{
    switch (column) {
    case Symbol:
        return row->symbol.symbol.isEmpty() ? tr("??") : row->symbol.symbol;
    case Binary:
        return row->symbol.binary;
    case Cost:
        return row->cost.samples;
    }
    Q_UNREACHABLE();
    return {};
}

QVariant BottomUpModel::displayToolTip(const Data::BottomUp* row, quint64 sampleCount)
{
    QString toolTip = tr("%1 in %2\ncost: %3 out of %4 total samples (%5%)").arg(
             Util::formatString(row->symbol.symbol), Util::formatString(row->symbol.binary),
             Util::formatCost(row->cost.samples), Util::formatCost(sampleCount), Util::formatCostRelative(row->cost.samples, sampleCount));
    return toolTip;
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

QString TopDownModel::headerToolTip(Columns column)
{
    switch (column) {
    case Symbol:
        return tr("The symbol's function name. May be empty when debug information is missing.");
    case Binary:
        return tr("The name of the executable the symbol resides in. May be empty when debug information is missing.");
    case InclusiveCost:
        return tr("The number of samples attributed to this symbol, both directly and indirectly. This includes the costs of all functions called by this symbol plus its self cost.");
    case SelfCost:
        return tr("The number of samples directly attributed to this symbol.");
    }
    Q_UNREACHABLE();
    return {};
}

QVariant TopDownModel::displayData(const Data::TopDown* row, Columns column)
{
    switch (column) {
    case Symbol:
        return row->symbol.symbol.isEmpty() ? tr("??") : row->symbol.symbol;
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

QVariant TopDownModel::displayToolTip(const Data::TopDown* row, quint64 sampleCount)
{
    QString toolTip = tr("%1 in %2\nself cost: %3 out of %4 total samples (%5%)\ninclusive cost: %6 out of %7 total samples (%8%)").arg(
             Util::formatString(row->symbol.symbol), Util::formatString(row->symbol.binary),
             Util::formatCost(row->selfCost.samples), Util::formatCost(sampleCount), Util::formatCostRelative(row->selfCost.samples, sampleCount),
             Util::formatCost(row->inclusiveCost.samples), Util::formatCost(sampleCount), Util::formatCostRelative(row->inclusiveCost.samples, sampleCount));
    return toolTip;
}
