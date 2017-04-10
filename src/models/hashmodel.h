/*
  hashmodel.h

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

#pragma once

#include <QAbstractTableModel>
#include <QHash>

template<typename Rows, typename ModelImpl>
class HashModel : public QAbstractTableModel
{
public:
    explicit HashModel(QObject* parent = nullptr)
        : QAbstractTableModel(parent)
    {}
    virtual ~HashModel() = default;

    int columnCount(const QModelIndex& parent = {}) const final override
    {
        return parent.isValid() ? 0 : ModelImpl::NUM_COLUMNS;
    }

    int rowCount(const QModelIndex &parent = {}) const final override
    {
        return parent.isValid() ? 0 : m_rows.size();
    }

    QVariant headerData(int section, Qt::Orientation orientation = Qt::Horizontal,
                        int role = Qt::DisplayRole) const final override
    {
        if (section < 0 || section > ModelImpl::NUM_COLUMNS || orientation != Qt::Horizontal) {
            return {};
        }

        const auto column = static_cast<typename ModelImpl::Columns>(section);
        return ModelImpl::headerCell(column, role);
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const final override
    {
        if (!hasIndex(index.row(), index.column(), index.parent())) {
            return {};
        }

        if (role == ModelImpl::TotalCostRole) {
            return m_sampleCount;
        }

        // TODO: optimize this? i.e. introduce a QVector of data, use hash only during construction?
        const auto& data = m_rows;
        auto it = data.begin();
        std::advance(it, index.row());

        const auto column = static_cast<typename ModelImpl::Columns>(index.column());
        return ModelImpl::cell(column, role, it.key(), it.value(), m_sampleCount);
    }

    using QAbstractTableModel::setData;
    void setData(const Rows& rows)
    {
        beginResetModel();
        m_rows = rows;
        endResetModel();
    }

    void setSampleCount(quint64 data)
    {
        beginResetModel();
        m_sampleCount = data;
        endResetModel();
    }

    QModelIndex indexForKey(const typename Rows::key_type& key, int column = 0) const
    {
        auto it = m_rows.find(key);
        if (it == m_rows.end()) {
            return {};
        }
        const int row = std::distance(m_rows.begin(), it);
        return index(row, column);
    }

protected:
    Rows m_rows;

private:
    quint64 m_sampleCount = 0;
};
