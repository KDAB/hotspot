/*
  callercalleemodel.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QVector>
#include <QAbstractItemModel>

#include "data.h"
#include "hashmodel.h"

class CallerCalleeModel : public HashModel<Data::CallerCalleeEntryMap, CallerCalleeModel>
{
    Q_OBJECT
public:
    explicit CallerCalleeModel(QObject* parent = nullptr);
    ~CallerCalleeModel();

    enum Columns {
        Symbol = 0,
        Binary,
        SelfCost,
        InclusiveCost,
        Callers,
        Callees,
    };
    enum {
        NUM_COLUMNS = Callees + 1
    };

    enum Roles {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        CalleesRole,
        CallersRole,
        SourceMapRole,
    };

    static QVariant headerCell(Columns column, int role);
    static QVariant cell(Columns column, int role, const Data::Symbol& symbol,
                         const Data::CallerCalleeEntry& entry, quint64 sampleCount);
    QModelIndex indexForSymbol(const Data::Symbol& symbol) const;
};

template<typename ModelImpl>
class SymbolCostModelImpl : public HashModel<Data::SymbolCostMap, ModelImpl>
{
public:
    explicit SymbolCostModelImpl(QObject* parent = nullptr)
        : HashModel<Data::SymbolCostMap, ModelImpl>(parent)
    {
    }

    virtual ~SymbolCostModelImpl() = default;

    enum Columns {
        Symbol = 0,
        Binary,
        Cost
    };
    enum {
        NUM_COLUMNS = Cost + 1
    };

    enum Roles {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        SymbolRole
    };

    static QVariant headerCell(Columns column, int role)
    {
        if (role == Qt::InitialSortOrderRole && column == Cost) {
            return Qt::DescendingOrder;
        }
        if (role != Qt::DisplayRole && role != Qt::ToolTipRole) {
            return {};
        }

        if (role == Qt::DisplayRole) {
            switch (column) {
                case Symbol:
                    return ModelImpl::symbolHeader();
                case Binary:
                    return QObject::tr("Binary");
                case Cost:
                    return QObject::tr("Cost");
            }
        } else if (role == Qt::ToolTipRole) {
            switch (column) {
                case Symbol:
                    return QObject::tr("The function name of the %1. May be empty when debug information is missing.").arg(ModelImpl::symbolHeader());
                case Binary:
                    return QObject::tr("The name of the executable the symbol resides in. May be empty when debug information is missing.");
                case Cost:
                    return QObject::tr("The symbol's inclusive cost, i.e. the number of samples attributed to this symbol, both directly and indirectly.");
            }
        }

        return {};
    }

    static QVariant cell(Columns column, int role, const Data::Symbol& symbol,
                         const Data::Cost& cost, quint64 sampleCount)
    {
        if (role == SortRole) {
            switch (column) {
                case Symbol:
                    return symbol.symbol;
                case Binary:
                    return symbol.binary;
                case Cost:
                    return cost.samples;
            }
        } else if (role == FilterRole) {
            // TODO: optimize this
            return QString(symbol.symbol + symbol.binary);
        } else if (role == Qt::DisplayRole) {
            // TODO: show fractional cost
            switch (column) {
                case Symbol:
                    return symbol.symbol.isEmpty() ? QObject::tr("??") : symbol.symbol;
                case Binary:
                    return symbol.binary;
                case Cost:
                    return cost.samples;
            }
        } else if (role == SymbolRole) {
            return QVariant::fromValue(symbol);
        } else if (role == Qt::ToolTipRole) {
            QString toolTip = QObject::tr("%1 in %2\ncost: %3 out of %4 total samples (%5%)").arg(
                     Util::formatString(symbol.symbol), Util::formatString(symbol.binary),
                     Util::formatCost(cost.samples), Util::formatCost(sampleCount), Util::formatCostRelative(cost.samples, sampleCount));
            return toolTip;
        }

        return {};
    }
};

class CallerModel : public SymbolCostModelImpl<CallerModel>
{
    Q_OBJECT
public:
    explicit CallerModel(QObject* parent = nullptr);
    ~CallerModel();

    static QString symbolHeader();
};

class CalleeModel : public SymbolCostModelImpl<CalleeModel>
{
    Q_OBJECT
public:
    explicit CalleeModel(QObject* parent = nullptr);
    ~CalleeModel();

    static QString symbolHeader();
};

template<typename ModelImpl>
class LocationCostModelImpl : public HashModel<Data::LocationCostMap, ModelImpl>
{
public:
    explicit LocationCostModelImpl(QObject* parent = nullptr)
        : HashModel<Data::LocationCostMap, ModelImpl>(parent)
    {
    }

    virtual ~LocationCostModelImpl() = default;

    enum Columns {
        Location = 0,
        Cost
    };
    enum {
        NUM_COLUMNS = Cost + 1
    };

    enum Roles {
        SortRole = Qt::UserRole,
        TotalCostRole,
        FilterRole,
        LocationRole
    };

    static QVariant headerCell(Columns column, int role)
    {
        if (role == Qt::InitialSortOrderRole && column == Cost) {
            return Qt::DescendingOrder;
        }

        if (role != Qt::DisplayRole && role != Qt::ToolTipRole) {
            return {};
        }

        if (role == Qt::DisplayRole) {
            switch (column) {
                case Location:
                    return QObject::tr("Location");
                case Cost:
                    return QObject::tr("Cost");
            }
        } else if (role == Qt::ToolTipRole) {
            switch (column) {
                case Location:
                    return QObject::tr("The source file name and line number where the cost was measured. May be empty when debug information is missing.");
                case Cost:
                    return QObject::tr("The number of samples directly attributed to this code location.");
            }
        }

        return {};
    }

    static QVariant cell(Columns column, int role, const QString& location,
                         const Data::Cost& cost, quint64 sampleCount)
    {
        if (role == SortRole) {
            switch (column) {
                case Location:
                    return location;
                case Cost:
                    return cost.samples;
            }
        } else if (role == FilterRole) {
            return location;
        } else if (role == Qt::DisplayRole) {
            // TODO: show fractional cost
            switch (column) {
                case Location:
                    return location.isEmpty() ? QObject::tr("??") : location;
                case Cost:
                    return cost.samples;
            }
        } else if (role == LocationRole) {
            return QVariant::fromValue(location);
        } else if (role == Qt::ToolTipRole) {
            QString toolTip = QObject::tr("%1\ncost: %2 out of %3 total samples (%4%)").arg(
                     Util::formatString(location), Util::formatCost(cost.samples), Util::formatCost(sampleCount), Util::formatCostRelative(cost.samples, sampleCount));
            return toolTip;
        }

        return {};
    }
};

class SourceMapModel : public LocationCostModelImpl<SourceMapModel>
{
    Q_OBJECT
public:
    explicit SourceMapModel(QObject* parent = nullptr);
    ~SourceMapModel();
};
