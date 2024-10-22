/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>

#include "data.h"
#include "hashmodel.h"

class ByFileModel : public HashModel<Data::ByFileEntryMap, ByFileModel>
{
    Q_OBJECT
public:
    explicit ByFileModel(QObject* parent = nullptr);
    ~ByFileModel();

    void setResults(const Data::ByFileResults& results);

    enum Columns
    {
        File = 0,
    };
    enum
    {
        NUM_BASE_COLUMNS = File + 1,
        InitialSortColumn = File + 1 // the first cost column
    };

    enum Roles
    {
        SortRole = Qt::UserRole,
        TotalCostRole,
        SelfCostsRole,
        InclusiveCostsRole,
        SourceMapRole,
        FileRole,
    };

    QVariant headerCell(int column, int role) const final override;
    QVariant cell(int column, int role, const QString& file, const Data::ByFileEntry& entry) const final override;
    int numColumns() const final override;
    QModelIndex indexForFile(const QString& file) const;

private:
    Data::ByFileResults m_results;
};
