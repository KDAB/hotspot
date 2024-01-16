/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
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

    typename Rows::key_type key(int row) const
    {
        return m_keys.value(row);
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
