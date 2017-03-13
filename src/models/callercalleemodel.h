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
                         const Data::CallerCalleeEntry& entry);
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
        if (role != Qt::DisplayRole) {
            return {};
        }

        switch (column) {
            case Symbol:
                return ModelImpl::symbolHeader();
            case Binary:
                return QObject::tr("Binary");
            case Cost:
                return QObject::tr("Cost");
        }

        return {};
    }

    static QVariant cell(Columns column, int role, const Data::Symbol& symbol,
                         const Data::Cost& cost)
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
        }

        // TODO: tooltips

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
        if (role != Qt::DisplayRole) {
            return {};
        }

        switch (column) {
            case Location:
                return QObject::tr("Location");
            case Cost:
                return QObject::tr("Cost");
        }

        return {};
    }

    static QVariant cell(Columns column, int role, const QString& location,
                         const Data::Cost& cost)
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
        }

        // TODO: tooltips

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
