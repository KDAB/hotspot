/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sourcecodemodel.h"
#include <QDir>
#include <QFile>

SourceCodeModel::SourceCodeModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

SourceCodeModel::~SourceCodeModel() = default;

void SourceCodeModel::clear()
{
    beginResetModel();
    m_sourceCode.clear();
    endResetModel();
}

void SourceCodeModel::setDisassembly(const DisassemblyOutput& disassemblyOutput)
{
    if (disassemblyOutput.sourceFileName.isEmpty())
        return;

    QFile file(m_sysroot + QDir::separator() + disassemblyOutput.sourceFileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    beginResetModel();

    int maxLineNumber = 0;
    int minLineNumber = INT_MAX;

    m_validLineNumbers.clear();

    for (const auto& line : disassemblyOutput.disassemblyLines) {
        if (line.sourceCodeLine == 0) {
            continue;
        }

        if (line.sourceCodeLine > maxLineNumber) {
            maxLineNumber = line.sourceCodeLine;
        }
        if (line.sourceCodeLine < minLineNumber) {
            minLineNumber = line.sourceCodeLine;
        }

        m_validLineNumbers.insert(line.sourceCodeLine);
    }

    Q_ASSERT(minLineNumber > 0);
    Q_ASSERT(minLineNumber < maxLineNumber);

    // -2: 1 for index, one for extra line with the symbol
    m_lineOffset = minLineNumber - 1;

    const auto lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));

    m_sourceCode.push_back(disassemblyOutput.symbol.prettySymbol);
    for (int i = minLineNumber - 1; i < maxLineNumber; i++) {
        m_sourceCode.push_back(lines[i]);
    }

    endResetModel();
}

QVariant SourceCodeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= COLUMN_COUNT)
        return {};
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    if (section == SourceCodeColumn)
        return tr("Source Code");

    if (section == SourceCodeLineNumber)
        return tr("Line");

    return {};
}

QVariant SourceCodeModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};

    if (index.row() > m_sourceCode.count() || index.row() < 0)
        return {};

    const auto& line = m_sourceCode.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
        if (index.column() == SourceCodeColumn)
            return line;

        if (index.column() == SourceCodeLineNumber) {
            return index.row() + m_lineOffset;
        }
    } else if (role == HighlightRole) {
        return index.row() + m_lineOffset == m_highlightLine;
    } else if (role == RainbowLineNumberRole) {
        int line = index.row() + m_lineOffset;
        if (m_validLineNumbers.contains(line))
            return line;
        return -1;
    }
    return {};
}

int SourceCodeModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

int SourceCodeModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_sourceCode.count();
}

void SourceCodeModel::updateHighlighting(int line)
{
    m_highlightLine = line;
    emit dataChanged(createIndex(0, Columns::SourceCodeColumn), createIndex(rowCount(), Columns::SourceCodeColumn));
}

int SourceCodeModel::lineForIndex(const QModelIndex& index) const
{
    return index.row() + m_lineOffset;
}

void SourceCodeModel::setSysroot(const QString& sysroot)
{
    m_sysroot = sysroot;
}
