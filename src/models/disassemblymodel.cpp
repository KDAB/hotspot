/*
    SPDX-FileCopyrightText: Darya Knysh <d.knysh@nips.ru>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QFontDatabase>
#include <QTextBlock>
#include <QTextDocument>

#include "disassemblymodel.h"
#include "hotspot-config.h"

#include "highlighter.hpp"
#include "search.h"
#include "sourcecodemodel.h"

DisassemblyModel::DisassemblyModel(KSyntaxHighlighting::Repository* repository, QObject* parent)
    : QAbstractItemModel(parent)
    , m_document(new QTextDocument(this))
    , m_highlighter(new Highlighter(m_document, repository, this))
    , m_disassemblyLines(nullptr)
{
    m_document->setUndoRedoEnabled(false);
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
        std::find_if(m_disassemblyLines.tree_begin(), m_disassemblyLines.tree_end(),
                     [address](const DisassemblyEntry& entry) { return entry.disassemblyLine().addr == address; });

    if (found != m_disassemblyLines.tree_end()) {
        return indexFromEntry(&(*found));
    }

    return {};
}

QModelIndex DisassemblyModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    DisassemblyEntry* line = static_cast<DisassemblyEntry*>(index.internalPointer());

    if (line == nullptr) {
        return QModelIndex();
    }

    auto parentItem = line->parent();

    if (parentItem == &m_disassemblyLines) {
        return QModelIndex();
    }

    return createIndex(parentItem->row(), 0, parentItem);
}

QModelIndex DisassemblyModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    DisassemblyEntry* parentItem;

    if (!parent.isValid()) {
        parentItem = const_cast<DisassemblyEntry*>(&m_disassemblyLines);
    } else {
        parentItem = static_cast<DisassemblyEntry*>(parent.internalPointer());
    }

    if (row < parentItem->childCount()) {
        return createIndex(row, column, parentItem->child(row));
    } else {
        return QModelIndex();
    }
}

void DisassemblyModel::setDisassembly(const DisassemblyOutput& disassemblyOutput,
                                      const Data::CallerCalleeResults& results)
{
    beginResetModel();

    m_data = disassemblyOutput;
    m_results = results;
    m_numTypes = results.selfCosts.numTypes();

    m_document->clear();
    m_disassemblyLines.clear();

    QTextCursor cursor(m_document);
    cursor.beginEditBlock();
    DisassemblyEntry* lastChild = nullptr;
    for (const auto& it : disassemblyOutput.disassemblyLines) {
        cursor.insertText(it.disassembly);
        cursor.insertBlock();
    }
    cursor.endEditBlock();

    m_document->setTextWidth(m_document->idealWidth());

    int linecounter = 0;
    QString function;
    for (const auto& it : disassemblyOutput.disassemblyLines) {
        auto textLine = m_document->findBlockByLineNumber(linecounter++).layout()->lineAt(0);

        // if symbol in not empty -> inlined function detected
        if (!it.symbol.isEmpty()) {
            // only collapse the same function
            if (lastChild && function == it.symbol) {
                lastChild->addChild({lastChild, it, textLine});
            } else {
                m_disassemblyLines.addChild(
                    {&m_disassemblyLines, {0, QLatin1String("%1 [inlined]").arg(it.symbol), {}, {}, {}}});
                lastChild = m_disassemblyLines.lastChild();
                Q_ASSERT(lastChild);
                lastChild->addChild({lastChild, it, textLine});
                function = it.symbol;
            }
        } else {
            lastChild = nullptr;
            m_disassemblyLines.addChild({&m_disassemblyLines, it, textLine});
        }
    }

    endResetModel();
}

QVariant DisassemblyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (section < 0 || section >= m_numTypes + COLUMN_COUNT)
        return {};
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};

    if (section == AddrColumn)
        return tr("Address");
    else if (section == DisassemblyColumn)
        return tr("Assembly / Disassembly");

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

    if (role == Qt::FontRole) {
        if (index.column() < COLUMN_COUNT)
            return QFontDatabase::systemFont(QFontDatabase::FixedFont);
        return {};
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == AddrColumn)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    DisassemblyEntry* entry = static_cast<DisassemblyEntry*>(index.internalPointer());
    Q_ASSERT(entry);
    auto line = entry->disassemblyLine();

    if (role == Qt::DisplayRole || role == CostRole || role == TotalCostRole || role == SyntaxHighlightRole
        || role == Qt::ToolTipRole) {
        if (role != Qt::ToolTipRole) {
            if (index.column() == AddrColumn) {
                if (!line.addr)
                    return {};
                return QString::number(line.addr, 16);
            } else if (index.column() == DisassemblyColumn) {
                if (role == SyntaxHighlightRole)
                    return QVariant::fromValue(entry->textLine());
                return line.disassembly;
            }
        }

        if (line.addr == 0) {
            return {};
        }

        const auto entry = m_results.entries.value(m_data.symbol);
        auto it = entry.offsetMap.find(line.addr);
        if (it != entry.offsetMap.end()) {
            int event = index.column() - COLUMN_COUNT;

            const auto& locationCost = it.value();
            const auto& costLine = locationCost.selfCost[event];
            const auto totalCost = m_results.selfCosts.totalCost(event);

            if (role == CostRole) {
                return costLine;
            } else if (role == TotalCostRole) {
                return totalCost;
            } else if (role == Qt::ToolTipRole) {
                auto tooltip = tr("addr: <tt>%1</tt><br/>assembly: <tt>%2</tt><br/>disassembly: <tt>%3</tt>")
                                   .arg(QString::number(line.addr, 16), line.disassembly);
                return Util::formatTooltip(tooltip, locationCost, m_results.selfCosts);
            }

            if (!costLine)
                return {};
            return Util::formatCostRelative(costLine, totalCost, true);
        } else {
            if (role == Qt::ToolTipRole)
                return tr("<qt><tt>%1</tt><hr/>No samples at this location.</qt>")
                    .arg(line.disassembly.toHtmlEscaped());
            else
                return QString();
        }
    } else if (role == DisassemblyModel::HighlightRole) {
        return line.fileLine.line == m_highlightLine;
    } else if (role == LinkedFunctionNameRole) {
        return line.linkedFunction.name;
    } else if (role == LinkedFunctionOffsetRole) {
        return line.linkedFunction.offset;
    } else if (role == RainbowLineNumberRole && line.addr) {
        // don't color inlined stuff because that is confusing
        // TODO find better solution like coloring in the color of the calling line
        if (line.symbol.isEmpty()) {
            return line.fileLine.line;
        }
        return -1;
    }

    return {};
}

int DisassemblyModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return COLUMN_COUNT + m_numTypes;
}

int DisassemblyModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return 0;
    if (parent.isValid()) {
        auto item = static_cast<DisassemblyEntry*>(parent.internalPointer());
        return item->childCount();
    }
    return m_disassemblyLines.childCount();
}

void DisassemblyModel::updateHighlighting(int line)
{
    m_highlightLine = line;
    emit dataChanged(createIndex(0, Columns::DisassemblyColumn), createIndex(rowCount(), Columns::DisassemblyColumn));
}

Data::FileLine DisassemblyModel::fileLineForIndex(const QModelIndex& index) const
{
    auto entry = reinterpret_cast<DisassemblyEntry*>(index.internalPointer());
    return entry->disassemblyLine().fileLine;
}

QModelIndex DisassemblyModel::indexForFileLine(const Data::FileLine& fileLine) const
{
    DisassemblyEntry* bestMatch = nullptr;
    qint64 bestCost = 0;
    const auto entry = m_results.entries.value(m_data.symbol);

    for (auto line = m_disassemblyLines.tree_begin(); line != m_disassemblyLines.tree_end(); line++) {
        if (line->disassemblyLine().fileLine != fileLine) {
            continue;
        }

        if (!bestMatch) {
            bestMatch = &(*line);
        }

        auto it = entry.offsetMap.find(line->disassemblyLine().addr);
        if (it != entry.offsetMap.end()) {
            const auto& locationCost = it.value();

            if (!bestCost || bestCost < locationCost.selfCost[0]) {
                bestMatch = &(*line);
                bestCost = locationCost.selfCost[0];
            }
        }
    }

    if (!bestMatch)
        return {};

    return indexFromEntry(bestMatch);
}

QModelIndex DisassemblyModel::indexFromEntry(DisassemblyEntry* entry) const
{
    Q_ASSERT(entry);

    int index = entry->parent()->findOffsetOf(entry);
    Q_ASSERT(index >= 0);

    return createIndex(index, 0, entry);
}

void DisassemblyModel::find(const QString& search, Direction direction, int offset)
{
    auto searchFunc = [&search](const DisassemblyOutput::DisassemblyLine& line) {
        return line.disassembly.indexOf(search, 0, Qt::CaseInsensitive) != -1;
    };

    int resultIndex = ::search(
        m_data.disassemblyLines, searchFunc, [this] { emit resultFound({}); }, direction, offset);

    if (resultIndex >= 0) {
        emit resultFound(createIndex(resultIndex, DisassemblyColumn));
    } else {
        emit resultFound({});
    }
}
