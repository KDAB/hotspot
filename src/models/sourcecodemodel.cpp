/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sourcecodemodel.h"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>
#include <iterator>
#include <limits>

#include "search.h"
#include <climits>

Q_LOGGING_CATEGORY(sourcecodemodel, "hotspot.sourcecodemodel", QtWarningMsg)

SourceCodeModel::SourceCodeModel(KSyntaxHighlighting::Repository* repository, QObject* parent)
    : QAbstractTableModel(parent)
    , m_highlightedText(repository)
{
    qRegisterMetaType<QTextLine>();
}

SourceCodeModel::~SourceCodeModel() = default;

void SourceCodeModel::clear()
{
    beginResetModel();
    m_highlightedText.setText({});
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

    if (disassemblyOutput.mainSourceFileName.isEmpty()) {
        return;
    }

    int maxLineNumber = 0;
    int minLineNumber = std::numeric_limits<int>::max();

    m_validLineNumbers.clear();

    m_selfCosts.initializeCostsFrom(results.selfCosts);
    m_inclusiveCosts.initializeCostsFrom(results.inclusiveCosts);

    m_mainSourceFileName = disassemblyOutput.mainSourceFileName;

    const auto entry = results.entries.find(disassemblyOutput.symbol);

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

    if (maxLineNumber == 0) {
        qCWarning(sourcecodemodel) << "failed to parse line numbers from disassembly output";
        return;
    }

    qCDebug(sourcecodemodel) << disassemblyOutput.mainSourceFileName << minLineNumber << maxLineNumber;

    Q_ASSERT(minLineNumber > 0);
    Q_ASSERT(minLineNumber < maxLineNumber);

    m_prettySymbol = disassemblyOutput.symbol.prettySymbol;
    m_startLine = minLineNumber - 1; // convert to index
    m_numLines = maxLineNumber - minLineNumber + 1; // include minLineNumber

    QFile file(disassemblyOutput.realSourceFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    const auto sourceCode = QString::fromUtf8(file.readAll());

    m_lines = sourceCode.split(QLatin1Char('\n'));

    m_highlightedText.setText(m_lines);
}

QVariant SourceCodeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= COLUMN_COUNT + m_selfCosts.numTypes() + m_inclusiveCosts.numTypes())
        return {};

    if ((role != Qt::DisplayRole && role != Qt::ToolTipRole) || orientation != Qt::Horizontal)
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

    const auto fileLine = Data::FileLine(m_mainSourceFileName, index.row() + m_startLine);
    if (role == FileLineRole) {
        return QVariant::fromValue(fileLine);
    } else if (role == Qt::ToolTipRole) {
        return Util::formatTooltip(fileLine, m_selfCosts, m_inclusiveCosts);
    }

    if (role == Qt::DisplayRole || role == CostRole || role == TotalCostRole || role == SyntaxHighlightRole) {
        if (index.column() == SourceCodeColumn) {
            if (index.row() == 0) {
                return m_prettySymbol;
            }

            const int lineNumber = m_startLine + index.row() - 1;
            if (role == SyntaxHighlightRole) {
                return QVariant::fromValue(m_highlightedText.lineAt(lineNumber));
            }
            return m_highlightedText.textAt(lineNumber);
        }

        if (index.column() == SourceCodeLineNumber) {
            return fileLine.line;
        }

        auto cost = [role, id = index.row() + m_startLine](int type, const Data::Costs& costs) -> QVariant {
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
        return index.row() + m_startLine == m_highlightLine;
    } else if (role == RainbowLineNumberRole) {
        const auto line = index.row() + m_startLine;
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
    // don't show the function name, when we have no source code
    if (m_numLines == 0)
        return 0;

    // 1 line for the function name + source codes lines
    return parent.isValid() ? 0 : m_numLines + 1;
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
    return {m_mainSourceFileName, index.row() + m_startLine};
}

QModelIndex SourceCodeModel::indexForFileLine(const Data::FileLine& fileLine) const
{
    if (fileLine.file != m_mainSourceFileName || fileLine.line < m_startLine
        || fileLine.line > m_startLine + m_numLines)
        return {};
    return index(fileLine.line - m_startLine, 0);
}

void SourceCodeModel::setSysroot(const QString& sysroot)
{
    m_sysroot = sysroot;
}

void SourceCodeModel::find(const QString& search, Direction direction, int current)
{
    auto searchFunc = [&search](const QString& line) { return line.indexOf(search, 0, Qt::CaseInsensitive) != -1; };

    auto endReached = [this] { emit searchEndReached(); };
    const int resultIndex = ::search(m_lines.cbegin() + m_startLine, m_lines.cbegin() + m_startLine + m_numLines,
                                     current, direction, searchFunc, endReached);

    if (resultIndex >= 0) {
        emit resultFound(createIndex(resultIndex + 1, SourceCodeColumn));
    } else {
        emit resultFound({});
    }
}

void SourceCodeModel::scrollToLine(const QString& lineNumber)
{
    const auto line = lineNumber.toInt();

    const auto offset = index(0, SourceCodeModel::SourceCodeLineNumber).data().value<int>();

    auto scrollToIndex = std::clamp(line - offset, 0, rowCount() - 1);

    emit resultFound(index(scrollToIndex, 0));
}
