/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "processlist.h"

class ProcessModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ProcessModel(QObject* parent = nullptr);
    virtual ~ProcessModel();

    void setProcesses(const ProcDataList& processes);
    void mergeProcesses(const ProcDataList& processes);
    ProcData dataForIndex(const QModelIndex& index) const;
    ProcData dataForRow(int row) const;
    QModelIndex indexForPid(const QString& pid) const;

    ProcDataList processes() const;

    void clear();

    enum Columns
    {
        PIDColumn,
        NameColumn,
        StateColumn,
        UserColumn,
        COLUMN_COUNT
    };

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    enum CustomRoles
    {
        PIDRole = Qt::UserRole,
        NameRole,
        StateRole,
        UserRole
    };

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    ProcDataList m_data;
};
