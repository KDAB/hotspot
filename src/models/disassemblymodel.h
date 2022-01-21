/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractTableModel>
#include "data.h"
#include "disassemblyoutput.h"

class DisassemblyModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DisassemblyModel(QObject *parent = nullptr);
    ~DisassemblyModel();

    void setDisassembly(const DisassemblyOutput& disassemblyOutput);
    void setResults(const Data::CallerCalleeResults& results);

    void clear();
    QModelIndex findIndexWithOffset(int offset);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    enum Columns
    {
        DisassemblyColumn,
        LinkedFunctionName,
        LinkedFunctionOffset,
        COLUMN_COUNT
    };

    enum CustomRoles
    {
        CostRole = Qt::UserRole,
        TotalCostRole = Qt::UserRole + 1,
    };
private:
    DisassemblyOutput m_data;
    Data::CallerCalleeResults m_results;
    int m_numTypes;
};
