/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "data.h"
#include "disassemblyoutput.h"

#include <memory>
#include <QAbstractTableModel>
#include <QTextLine>

class QTextDocument;

class Highlighter;

Q_DECLARE_METATYPE(QTextLine)

class SourceCodeModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit SourceCodeModel(QObject* parent = nullptr);
    ~SourceCodeModel();

    void clear();
    void setDisassembly(const DisassemblyOutput& disassemblyOutput);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int lineForIndex(const QModelIndex& index) const;

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
    };

public slots:
    void updateHighlighting(int line);
    void setSysroot(const QString& sysroot);
    void setCallerCalleeResults(const Data::CallerCalleeResults& results);

private:
    QString m_sysroot;
    QSet<int> m_validLineNumbers;
    QTextDocument* m_document = nullptr;
    Highlighter* m_highlighter = nullptr;
    Data::CallerCalleeResults m_callerCalleeResults;
    Data::Costs m_costs;
    int m_numTypes = 0;
    int m_lineOffset = 0;
    int m_startLine = 0;
    int m_numLines = 0;
    int m_highlightLine = 0;
};
