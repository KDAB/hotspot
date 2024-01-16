/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <data.h>
#include <QAbstractTableModel>

class FrequencyModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit FrequencyModel(QObject* parent = nullptr);
    ~FrequencyModel();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setResults(const Data::FrequencyResults& results);

private:
    QVector<Data::PerCostFrequencyData> m_frequencyData;
};
