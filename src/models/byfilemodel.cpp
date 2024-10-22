/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "byfilemodel.h"
#include "../util.h"

#include <QDebug>

ByFileModel::ByFileModel(QObject* parent)
    : HashModel(parent)
{
}

ByFileModel::~ByFileModel() = default;

void ByFileModel::setResults(const Data::ByFileResults& results)
{
    m_results = results;
    setRows(results.entries);
}

QVariant ByFileModel::headerCell(int column, int role) const
{
    if (role == Qt::DisplayRole) {
        switch (column) {
        case File:
            return tr("File");
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.selfCosts.numTypes()) {
            return tr("%1 (self)").arg(m_results.selfCosts.typeName(column));
        }
        column -= m_results.selfCosts.numTypes();
        return tr("%1 (incl.)").arg(m_results.inclusiveCosts.typeName(column));
    } else if (role == Qt::ToolTipRole) {
        switch (column) {
        case File:
            return tr("The name of the file. May be empty when debug information is missing.");
        }

        column -= NUM_BASE_COLUMNS;
        if (column < m_results.selfCosts.numTypes()) {
            return tr("The aggregated sample costs directly attributed to this file.");
        }
        return tr("The aggregated sample costs attributed to this file, both directly and indirectly. This includes "
                  "the costs of all functions called by this file plus its self cost.");
    }

    return {};
}

QVariant ByFileModel::cell(int column, int role, const QString& file, const Data::ByFileEntry& entry) const
{
    if (role == FileRole) {
        return QVariant::fromValue(file);
    } else if (role == SortRole) {
        switch (column) {
        case File:
            return Util::formatString(file);
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.selfCosts.numTypes()) {
            return m_results.selfCosts.cost(column, entry.id);
        }
        column -= m_results.selfCosts.numTypes();
        return m_results.inclusiveCosts.cost(column, entry.id);
    } else if (role == TotalCostRole && column >= NUM_BASE_COLUMNS) {
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.selfCosts.numTypes()) {
            return m_results.selfCosts.totalCost(column);
        }

        column -= m_results.selfCosts.numTypes();
        return m_results.inclusiveCosts.totalCost(column);
    } else if (role == Qt::DisplayRole) {
        switch (column) {
        case File:
            return Util::formatString(file);
        }
        column -= NUM_BASE_COLUMNS;
        if (column < m_results.selfCosts.numTypes()) {
            return Util::formatCostRelative(m_results.selfCosts.cost(column, entry.id),
                                            m_results.selfCosts.totalCost(column), true);
        }
        column -= m_results.selfCosts.numTypes();
        return Util::formatCostRelative(m_results.inclusiveCosts.cost(column, entry.id),
                                        m_results.inclusiveCosts.totalCost(column), true);
    } else if (role == SourceMapRole) {
        return QVariant::fromValue(entry.sourceMap);
    } else if (role == SelfCostsRole) {
        return QVariant::fromValue(m_results.selfCosts);
    } else if (role == InclusiveCostsRole) {
        return QVariant::fromValue(m_results.inclusiveCosts);
    } else if (role == Qt::ToolTipRole) {
        return Util::formatFileTooltip(entry.id, file, m_results.selfCosts, m_results.inclusiveCosts);
    }

    return {};
}

QModelIndex ByFileModel::indexForFile(const QString& file) const
{
    return indexForKey(file);
}

int ByFileModel::numColumns() const
{
    return NUM_BASE_COLUMNS + m_results.inclusiveCosts.numTypes() + m_results.selfCosts.numTypes();
}
