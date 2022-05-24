/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "disassemblyoutput.h"
#include <QAbstractTableModel>
#include <QTextLine>

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
    };

public slots:
    void updateHighlighting(int line);
    void setSysroot(const QString& sysroot);

private:
    QString m_sysroot;
    QSet<int> m_validLineNumbers;
    QStringList m_sourceCode;
    int m_lineOffset = 0;
    int m_highlightLine = 0;
};
