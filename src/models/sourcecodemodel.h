/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "data.h"
#include "disassemblyoutput.h"
#include "highlightedtext.h"

#include <memory>
#include <QAbstractTableModel>
#include <QTextLine>

namespace KSyntaxHighlighting {
class Repository;
class Definition;
}

enum class Direction;

Q_DECLARE_METATYPE(QTextLine)

class SourceCodeModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit SourceCodeModel(KSyntaxHighlighting::Repository* repository, QObject* parent = nullptr);
    ~SourceCodeModel() override;

    void clear();
    void setDisassembly(const DisassemblyOutput& disassemblyOutput, const Data::CallerCalleeResults& results);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    Data::FileLine fileLineForIndex(const QModelIndex& index) const;
    QModelIndex indexForFileLine(const Data::FileLine& line) const;

    HighlightedText* highlightedText()
    {
        return &m_highlightedText;
    }

    enum Columns
    {
        SourceCodeLineNumber,
        SourceCodeColumn,
        COLUMN_COUNT
    };

    enum CustomRoles
    {
        RainbowLineNumberRole = Qt::UserRole,
        HighlightRole,
        CostRole,
        TotalCostRole,
        SyntaxHighlightRole,
        FileLineRole,
    };

signals:
    void resultFound(const QModelIndex& index);
    void searchEndReached();

public slots:
    void updateHighlighting(int line);
    void setSysroot(const QString& sysroot);

    void find(const QString& search, Direction direction, int current);
    void scrollToLine(const QString& lineNumber);

private:
    QString m_sysroot;
    QSet<int> m_validLineNumbers;
    HighlightedText m_highlightedText;
    Data::Costs m_selfCosts;
    Data::Costs m_inclusiveCosts;
    QString m_mainSourceFileName;
    QString m_prettySymbol;
    int m_startLine = 0;
    int m_numLines = 0;
    QStringList m_lines;
    int m_highlightLine = 0;
};
