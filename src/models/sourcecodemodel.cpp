/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sourcecodemodel.h"

#include <QDir>
#include <QFile>
#include <QTextBlock>
#include <QTextDocument>

#include "highlighter.hpp"

SourceCodeModel::SourceCodeModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_document(new QTextDocument(this))
    , m_highlighter(new Highlighter(m_document, this))
{
    qRegisterMetaType<QTextLine>();
}

SourceCodeModel::~SourceCodeModel() = default;

void SourceCodeModel::clear()
{
    beginResetModel();
    m_document->clear();
    endResetModel();
}

void SourceCodeModel::setDisassembly(const DisassemblyOutput& disassemblyOutput)
{
    m_numLines = 0;
    m_numTypes = 0;

    if (disassemblyOutput.mainSourceFileName.isEmpty())
        return;

    QFile file(m_sysroot + QDir::separator() + disassemblyOutput.mainSourceFileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    beginResetModel();

    int maxLineNumber = 0;
    int minLineNumber = INT_MAX;

    m_validLineNumbers.clear();

    auto entry = m_callerCalleeResults.entry(disassemblyOutput.symbol);
    m_costs = {};
    m_costs.initializeCostsFrom(m_callerCalleeResults.selfCosts);

    m_numTypes = m_costs.numTypes();

    const auto sourceCode = QString::fromUtf8(file.readAll());

    m_document->setPlainText(sourceCode);
    m_document->setTextWidth(m_document->idealWidth());

    m_highlighter->setDefinitionForFilename(disassemblyOutput.mainSourceFileName);

    for (const auto& line : disassemblyOutput.disassemblyLines) {
        if (line.sourceCodeLine == 0 || line.sourceFileName != disassemblyOutput.mainSourceFileName) {
            continue;
        }

        if (line.sourceCodeLine > maxLineNumber) {
            maxLineNumber = line.sourceCodeLine;
        }
        if (line.sourceCodeLine < minLineNumber) {
            minLineNumber = line.sourceCodeLine;
        }

        auto it = entry.offsetMap.find(line.addr);
        if (it != entry.offsetMap.end()) {
            const auto& locationCost = it.value();
            const auto& costLine = locationCost.selfCost;
            const auto totalCost = m_callerCalleeResults.selfCosts.totalCosts();

            m_costs.add(line.sourceCodeLine, costLine);
        }

        m_validLineNumbers.insert(line.sourceCodeLine);
    }

    Q_ASSERT(minLineNumber > 0);
    Q_ASSERT(minLineNumber < maxLineNumber);

    m_startLine = minLineNumber - 2;
    m_lineOffset = minLineNumber - 1;
    m_numLines = maxLineNumber - m_startLine;

    QTextCursor cursor(m_document->findBlockByLineNumber(m_startLine));
    cursor.select(QTextCursor::SelectionType::LineUnderCursor);
    cursor.removeSelectedText();
    cursor.insertText(disassemblyOutput.symbol.prettySymbol);

    endResetModel();
}

QVariant SourceCodeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= COLUMN_COUNT + m_numTypes)
        return {};
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    if (section == SourceCodeColumn)
        return tr("Source Code");

    if (section == SourceCodeLineNumber)
        return tr("Line");

    if (section - COLUMN_COUNT <= m_numTypes) {
        return m_costs.typeName(section - COLUMN_COUNT);
    }

    return {};
}

QVariant SourceCodeModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};

    if (index.row() > m_numLines || index.row() < 0)
        return {};

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole || role == CostRole || role == TotalCostRole
        || role == SyntaxHighlightRole) {
        if (index.column() == SourceCodeColumn) {
            const auto block = m_document->findBlockByLineNumber(index.row() + m_startLine);
            if (!block.isValid())
                return {};
            if (role == SyntaxHighlightRole)
                return QVariant::fromValue(block.layout()->lineAt(0));
            return block.text();
        }

        if (index.column() == SourceCodeLineNumber) {
            return index.row() + m_lineOffset;
        }

        if (index.column() - COLUMN_COUNT < m_numTypes) {
            const auto cost = m_costs.cost(index.column() - COLUMN_COUNT, index.row() + m_lineOffset);
            const auto totalCost = m_costs.totalCost(index.column() - COLUMN_COUNT);
            if (role == CostRole) {
                return cost;
            }
            if (role == TotalCostRole) {
                return totalCost;
            }

            return Util::formatCostRelative(cost, totalCost, true);
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
    return parent.isValid() ? 0 : COLUMN_COUNT + m_numTypes;
}

int SourceCodeModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_numLines;
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

void SourceCodeModel::setCallerCalleeResults(const Data::CallerCalleeResults& results)
{
    beginResetModel();
    m_callerCalleeResults = results;
    endResetModel();
}
