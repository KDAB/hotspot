/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>

#include "data.h"

class EventModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit EventModel(QObject* parent = nullptr);
    virtual ~EventModel();

    enum Columns
    {
        ThreadColumn = 0,
        EventsColumn,
        NUM_COLUMNS
    };
    enum Roles
    {
        EventsRole = Qt::UserRole,
        MaxTimeRole,
        MinTimeRole,
        ThreadStartRole,
        ThreadEndRole,
        ThreadNameRole,
        ThreadIdRole,
        ProcessIdRole,
        CpuIdRole,
        NumProcessesRole,
        NumThreadsRole,
        NumCpusRole,
        MaxCostRole,
        SortRole,
        TotalCostsRole,
        EventResultsRole,
        IsFavoriteRole,
    };

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;

    using QAbstractItemModel::setData;
    void setData(const Data::EventResults& data);

    Data::TimeRange timeRange() const;

    struct Process
    {
        Process(qint32 pid = Data::INVALID_PID, const QVector<qint32>& threads = {}, const QString& name = {})
            : pid(pid)
            , threads(threads)
            , name(name)
        {
        }
        qint32 pid;
        QVector<qint32> threads;
        QString name;
    };

public:
    void addToFavorites(const QModelIndex& index);
    void removeFromFavorites(const QModelIndex& index);

private:
    Data::EventResults m_data;
    QVector<Process> m_processes;
    QVector<QModelIndex> m_favourites;
    Data::TimeRange m_time;
    quint64 m_totalOnCpuTime = 0;
    quint64 m_totalOffCpuTime = 0;
    quint64 m_totalEvents = 0;
    quint64 m_maxCost = 0;
};

Q_DECLARE_TYPEINFO(EventModel::Process, Q_MOVABLE_TYPE);
