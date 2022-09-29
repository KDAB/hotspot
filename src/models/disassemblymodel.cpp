/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFont>
#include <QTextBlock>
#include <QTextDocument>

#include "disassemblymodel.h"
#include "hotspot-config.h"

#include "highlighter.hpp"
#include "sourcecodemodel.h"

DisassemblyModel::DisassemblyModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_document(new QTextDocument(this))
    , m_highlighter(new Highlighter(m_document, this))
{
    m_document->setDefaultFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

DisassemblyModel::~DisassemblyModel() = default;

void DisassemblyModel::clear()
{
    beginResetModel();
    m_data = {};
    endResetModel();
}

QModelIndex DisassemblyModel::findIndexWithOffset(int offset)
{
    quint64 address = m_data.disassemblyLines[0].addr + offset;

    const auto& found =
        std::find_if(m_data.disassemblyLines.begin(), m_data.disassemblyLines.end(),
                     [address](const DisassemblyOutput::DisassemblyLine& line) { return line.addr == address; });

    if (found != m_data.disassemblyLines.end()) {
        return createIndex(std::distance(m_data.disassemblyLines.begin(), found), DisassemblyColumn);
    }
    return {};
}

void DisassemblyModel::setDisassembly(const DisassemblyOutput& disassemblyOutput)
{
    beginResetModel();
    m_data = disassemblyOutput;

    QTextCursor cursor(m_document);
    for (const auto& it : disassemblyOutput.disassemblyLines) {
        cursor.insertText(it.disassembly);
        cursor.insertBlock();
    }

    m_document->setTextWidth(m_document->idealWidth());

    m_highlighter->setDefinitionForName(QStringLiteral("GNU Assembler"));

    endResetModel();
}

void DisassemblyModel::setResults(const Data::CallerCalleeResults& results)
{
    beginResetModel();
    m_results = results;
    m_numTypes = results.selfCosts.numTypes();
    endResetModel();
}

QVariant DisassemblyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= m_numTypes + COLUMN_COUNT)
        return {};
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    if (section == DisassemblyColumn)
        return tr("Assembly");

    if (section - COLUMN_COUNT <= m_numTypes)
        return m_results.selfCosts.typeName(section - COLUMN_COUNT);

    return {};
}

QVariant DisassemblyModel::data(const QModelIndex& index, int role) const
{
    if (!hasIndex(index.row(), index.column(), index.parent()))
        return {};

    if (index.row() > m_data.disassemblyLines.count() || index.row() < 0)
        return {};

    const auto& data = m_data.disassemblyLines.at(index.row());

    if (role == Qt::DisplayRole || role == CostRole || role == TotalCostRole || role == SyntaxHighlightRole
        || role == Qt::ToolTipRole) {
        if (index.column() == DisassemblyColumn) {
            const auto block = m_document->findBlockByLineNumber(index.row());
            if (role == SyntaxHighlightRole)
                return QVariant::fromValue(block.layout()->lineAt(0));
            return block.text();
        }

        if (data.addr == 0) {
            return {};
        }

        auto results = m_results;
        auto entry = results.entry(m_data.symbol);
        auto it = entry.offsetMap.find(data.addr);
        if (it != entry.offsetMap.end()) {
            int event = index.column() - COLUMN_COUNT;

            const auto& locationCost = it.value();
            const auto& costLine = locationCost.selfCost[event];
            const auto totalCost = m_results.selfCosts.totalCost(event);

            if (role == CostRole)
                return costLine;
            if (role == TotalCostRole)
                return totalCost;
            if (role == Qt::ToolTipRole)
                return Util::formatTooltip(data.disassembly, locationCost, m_results.selfCosts);

            return Util::formatCostRelative(costLine, totalCost, true);
        } else {
            if (role == Qt::ToolTipRole)
                return tr("<qt><tt>%1</tt><hr/>No samples at this location.</qt>")
                    .arg(data.disassembly.toHtmlEscaped());
            else
                return QString();
        }
    } else if (role == DisassemblyModel::HighlightRole) {
        return data.sourceCodeLine == m_highlightLine;
    } else if (role == LinkedFunctionNameRole) {
        return data.linkedFunction.name;
    } else if (role == LinkedFunctionOffsetRole) {
        return data.linkedFunction.offset;
    } else if (role == RainbowLineNumberRole) {
        return data.sourceCodeLine;
    }

    return {};
}

int DisassemblyModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT + m_numTypes;
}

int DisassemblyModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_data.disassemblyLines.count();
}

void DisassemblyModel::updateHighlighting(int line)
{
    m_highlightLine = line;
    emit dataChanged(createIndex(0, Columns::DisassemblyColumn), createIndex(rowCount(), Columns::DisassemblyColumn));
}

int DisassemblyModel::lineForIndex(const QModelIndex& index) const
{
    return m_data.disassemblyLines[index.row()].sourceCodeLine;
}
