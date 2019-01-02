/*
  hashmodel.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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
#include <QVector>

template<typename Rows, typename ModelImpl>
class HashModel : public QAbstractTableModel
{
public:
    explicit HashModel(QObject* parent = nullptr)
        : QAbstractTableModel(parent)
    {
    }
    virtual ~HashModel() = default;

    int columnCount(const QModelIndex& parent = {}) const final override
    {
        return parent.isValid() ? 0 : numColumns();
    }

    int rowCount(const QModelIndex& parent = {}) const final override
    {
        return parent.isValid() ? 0 : m_keys.size();
    }

    QVariant headerData(int section, Qt::Orientation orientation = Qt::Horizontal,
                        int role = Qt::DisplayRole) const final override
    {
        if (section < 0 || section > numColumns() || orientation != Qt::Horizontal) {
            return {};
        }

        return headerCell(section, role);
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const final override
    {
        if (!hasIndex(index.row(), index.column(), index.parent())) {
            return {};
        }

        const auto& key = m_keys.value(index.row());
        const auto& value = m_values.value(index.row());

        return cell(index.column(), role, key, value);
    }

    QModelIndex indexForKey(const typename Rows::key_type& key, int column = 0) const
    {
        auto it = std::find(m_keys.begin(), m_keys.end(), key);
        if (it == m_keys.end()) {
            return {};
        }
        const int row = std::distance(m_keys.begin(), it);
        return index(row, column);
    }

protected:
    void setRows(const Rows& rows)
    {
        beginResetModel();
        m_keys.reserve(rows.size());
        m_values.reserve(rows.size());
        m_keys.clear();
        m_values.clear();
        for (auto it = rows.constBegin(), end = rows.constEnd(); it != end; ++it) {
            m_keys.push_back(it.key());
            m_values.push_back(it.value());
        }
        endResetModel();
    }

    virtual QVariant headerCell(int column, int role) const = 0;
    virtual QVariant cell(int column, int role, const typename Rows::key_type& key,
                          const typename Rows::mapped_type& entry) const = 0;
    virtual int numColumns() const = 0;

    QVector<typename Rows::key_type> m_keys;
    QVector<typename Rows::mapped_type> m_values;
};
