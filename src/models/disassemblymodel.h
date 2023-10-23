/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>
#include <QAbstractTableModel>
#include <QTextLine>

#include "data.h"
#include "disassemblyoutput.h"

class QTextDocument;
class Highlighter;

namespace KSyntaxHighlighting {
class Definition;
class Repository;
}

enum class Direction;

class DisassemblyModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit DisassemblyModel(KSyntaxHighlighting::Repository* repository, QObject* parent = nullptr);
    ~DisassemblyModel() override;

    void setDisassembly(const DisassemblyOutput& disassemblyOutput, const Data::CallerCalleeResults& results);

    void clear();
    QModelIndex findIndexWithOffset(int offset);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QSize span(const QModelIndex& index) const override;

    Data::FileLine fileLineForIndex(const QModelIndex& index) const;
    QModelIndex indexForFileLine(const Data::FileLine& line) const;

    Highlighter* highlighter() const
    {
        return m_highlighter;
    }

    enum Columns
    {
        AddrColumn,
        BranchColumn,
        DisassemblyColumn,
        COLUMN_COUNT
    };

    enum CustomRoles
    {
        CostRole = Qt::UserRole,
        TotalCostRole = Qt::UserRole + 1,
        HighlightRole,
        AddrRole,
        LinkedFunctionNameRole,
        LinkedFunctionOffsetRole,
        RainbowLineNumberRole,
        SyntaxHighlightRole,
    };

signals:
    void resultFound(QModelIndex index);
    void searchEndReached();

public slots:
    void updateHighlighting(int line);
    void find(const QString& search, Direction direction, int offset);

private:
    QTextDocument* m_document;
    Highlighter* m_highlighter;
    DisassemblyOutput m_data;
    Data::CallerCalleeResults m_results;
    int m_numTypes = 0;
    int m_highlightLine = 0;
};
