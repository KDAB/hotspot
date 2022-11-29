/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sourcecodemodel.h"

#include <QDir>
#include <QFile>
#include <QScopeGuard>
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

void SourceCodeModel::setDisassembly(const DisassemblyOutput& disassemblyOutput,
                                     const Data::CallerCalleeResults& results)
{
    beginResetModel();
    auto guard = qScopeGuard([this]() { endResetModel(); });

    m_selfCosts = {};
    m_inclusiveCosts = {};
    m_numLines = 0;

    if (disassemblyOutput.mainSourceFileName.isEmpty())
        return;

    QFile file(m_sysroot + QDir::separator() + disassemblyOutput.mainSourceFileName);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    int maxLineNumber = 0;
    int minLineNumber = INT_MAX;

    m_validLineNumbers.clear();

    m_selfCosts.initializeCostsFrom(results.selfCosts);
    m_inclusiveCosts.initializeCostsFrom(results.inclusiveCosts);

    m_mainSourceFileName = disassemblyOutput.mainSourceFileName;

    const auto sourceCode = QString::fromUtf8(file.readAll());

    m_document->setPlainText(sourceCode);
    m_document->setTextWidth(m_document->idealWidth());

    m_highlighter->setDefinitionForFilename(disassemblyOutput.mainSourceFileName);

    for (const auto& line : disassemblyOutput.disassemblyLines) {
        if (line.fileLine.line == 0 || line.fileLine.file != disassemblyOutput.mainSourceFileName) {
            continue;
        }

        if (line.fileLine.line > maxLineNumber) {
            maxLineNumber = line.fileLine.line;
        }
        if (line.fileLine.line < minLineNumber) {
            minLineNumber = line.fileLine.line;
        }

        if (m_validLineNumbers.contains(line.fileLine.line))
            continue;

        const auto entry = results.entries.find(disassemblyOutput.symbol);
        if (entry != results.entries.end()) {
            const auto it = entry->sourceMap.find(line.fileLine);
            if (it != entry->sourceMap.end()) {
                const auto& locationCost = it.value();

                m_selfCosts.add(line.fileLine.line, locationCost.selfCost);
                m_inclusiveCosts.add(line.fileLine.line, locationCost.inclusiveCost);
            }
        }

        m_validLineNumbers.insert(line.fileLine.line);
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
}

QVariant SourceCodeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= COLUMN_COUNT + m_selfCosts.numTypes() + m_inclusiveCosts.numTypes())
        return {};

    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    if (section == SourceCodeColumn)
        return tr("Source Code");

    if (section == SourceCodeLineNumber)
        return tr("Line");

    section -= COLUMN_COUNT;
    if (section < m_selfCosts.numTypes())
        return tr("%1 (self)").arg(m_selfCosts.typeName(section));

    section -= m_selfCosts.numTypes();
    return tr("%1 (incl.)").arg(m_inclusiveCosts.typeName(section));
}

QVariant SourceCodeModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};

    if (index.row() > m_numLines || index.row() < 0)
        return {};

    if (role == Qt::FontRole) {
        if (index.column() == SourceCodeColumn)
            return QFontDatabase::systemFont(QFontDatabase::FixedFont);
        return {};
    }

    const auto fileLine = Data::FileLine(m_mainSourceFileName, index.row() + m_lineOffset);
    if (role == FileLineRole) {
        return QVariant::fromValue(fileLine);
    } else if (role == Qt::ToolTipRole) {
        return Util::formatTooltip(fileLine, m_selfCosts, m_inclusiveCosts);
    }

    if (role == Qt::DisplayRole || role == CostRole || role == TotalCostRole || role == SyntaxHighlightRole) {
        if (index.column() == SourceCodeColumn) {
            const auto block = m_document->findBlockByLineNumber(index.row() + m_startLine);
            if (!block.isValid())
                return {};
            if (role == SyntaxHighlightRole)
                return QVariant::fromValue(block.layout()->lineAt(0));
            return block.text();
        }

        if (index.column() == SourceCodeLineNumber) {
            return fileLine.line;
        }

        auto cost = [role, id = index.row() + m_lineOffset](int type, const Data::Costs& costs) -> QVariant {
            const auto cost = costs.cost(type, id);
            const auto totalCost = costs.totalCost(type);
            if (role == CostRole) {
                return cost;
            }
            if (role == TotalCostRole) {
                return totalCost;
            }

            if (!cost)
                return {};
            return Util::formatCostRelative(cost, totalCost, true);
        };
        auto column = index.column() - COLUMN_COUNT;
        if (column < m_selfCosts.numTypes())
            return cost(column, m_selfCosts);
        return cost(column - m_selfCosts.numTypes(), m_inclusiveCosts);
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
    return parent.isValid() ? 0 : COLUMN_COUNT + m_selfCosts.numTypes() + m_inclusiveCosts.numTypes();
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

Data::FileLine SourceCodeModel::fileLineForIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};
    return {m_mainSourceFileName, index.row() + m_lineOffset};
}

QModelIndex SourceCodeModel::indexForFileLine(const Data::FileLine& fileLine) const
{
    if (fileLine.file != m_mainSourceFileName || fileLine.line < m_lineOffset
        || fileLine.line >= m_lineOffset + m_numLines)
        return {};
    return index(fileLine.line - m_lineOffset, 0);
}

void SourceCodeModel::setSysroot(const QString& sysroot)
{
    m_sysroot = sysroot;
}
