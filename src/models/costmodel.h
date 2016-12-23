/*
  costmodel.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

#include <QAbstractItemModel>

#include "framedata.h"

class CostModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    CostModel(QObject* parent = nullptr);
    ~CostModel();

    enum Columns {
        // TODO: extend
        Symbol = 0,
        SelfCost,
        InclusiveCost,
        NUM_COLUMNS
    };

    int columnCount(const QModelIndex& parent) const override;
    int rowCount(const QModelIndex& parent) const override;

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QModelIndex parent(const QModelIndex& child) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void setData(const FrameData& data);

private:
    const FrameData* itemFromIndex(const QModelIndex& index) const;
    QModelIndex indexFromItem(const FrameData* item, int column) const;

    FrameData m_root;
};
