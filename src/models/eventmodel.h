/*
  eventmodel.h

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

#pragma once

#include <QAbstractTableModel>

#include "data.h"

class EventModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    EventModel(QObject* parent = nullptr);
    virtual ~EventModel();

    enum Columns {
        ThreadColumn = 0,
        EventsColumn,
        NUM_COLUMNS
    };
    enum Roles {
        EventsRole = Qt::UserRole,
        MaxTimeRole,
        MinTimeRole,
        ThreadStartRole,
        ThreadEndRole,
        MaxCostRole
    };

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    using QAbstractTableModel::setData;
    void setData(const Data::EventResults& data);

private:
    Data::EventResults m_data;
    quint64 m_minTime = 0;
    quint64 m_maxTime = 0;
    quint64 m_totalEvents = 0;
    quint64 m_maxCost = 0;
};
