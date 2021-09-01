/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "callgraphgenerator.h"

#include <QTextStream>
#include <QUuid>

QHash<Data::Symbol, QString> writeGraph(QTextStream& stream, const Data::Symbol& symbol,
                                        Data::CallerCalleeResults& results, float thresholdPercent,
                                        const QString& fontColor)
{
    auto settings = Settings::instance();
    const auto parentId = QUuid::createUuid().toString(QUuid::Id128);

    stream << "digraph callgraph {\n";
    stream << "node [shape=box, fontname=\"monospace\", fontcolor=\"" << fontColor << "\", style=filled, color=\""
           << settings->callgraphColor().name() << "\"]\n";

    stream << "node" << parentId << " [label=\"";
    if (symbol.prettySymbol.isEmpty()) {
        stream << "??";
    } else {
        stream << symbol.prettySymbol;
    }
    stream << "\", color=\"" << settings->callgraphActiveColor().name() << "\"]\n";

    QHash<Data::Symbol, QString> symbolToIdLookup;
    symbolToIdLookup.insert(symbol, parentId);;

    resultsToDot(settings->callgraphParentDepth(), Direction::Caller, symbol, results, parentId, stream,
                 symbolToIdLookup, thresholdPercent);
    resultsToDot(settings->callgraphChildDepth(), Direction::Callee, symbol, results, parentId, stream,
                 symbolToIdLookup, thresholdPercent);

    stream << "}\n";
    return symbolToIdLookup;
}

void resultsToDot(int height, Direction direction, const Data::Symbol& symbol, Data::CallerCalleeResults& results,
                  const QString& parent, QTextStream& stream, QHash<Data::Symbol, QString>& nodeIdLookup,
                  float thresholdPercent)
{
    if (height == 0) {
        return;
    }

    if (symbol.prettySymbol.isEmpty())
        return;

    if (results.selfCosts.numTypes() == 0) {
        return;
    }

    const auto entry = results.entry(symbol);

    auto addNode = [&stream](const QString& id, const QString& label) {
        stream << "node" << id << " [label=\"" << label << "\"]\n";
    };
    auto connectNodes = [&stream, direction](const QString& parent, const QString& child) {
        if (direction == Direction::Callee) {
            stream << "node" << parent << " -> node" << child << "\n";
        } else {
            stream << "node" << child << " -> node" << parent << "\n";
        }
    };

    const auto totalCost = results.selfCosts.totalCost(0);
    auto map = direction == Direction::Callee ? entry.callees : entry.callers;

    for (auto it = map.begin(); it != map.end(); it++) {
        const auto cost = it.value()[0];
        if (static_cast<double>(cost) / totalCost < thresholdPercent) {
            continue;
        }

        auto& key = it.key();
        auto idIt = nodeIdLookup.find(key);
        if (idIt == nodeIdLookup.end()) {
            idIt = nodeIdLookup.insert(key, QUuid::createUuid().toString(QUuid::Id128));
            addNode(idIt.value(), key.prettySymbol.isEmpty() ? QStringLiteral("??") : key.prettySymbol);
        }
        const auto nodeId = idIt.value();

        connectNodes(parent, nodeId);
        resultsToDot(height - 1, direction, key, results, nodeId, stream, nodeIdLookup, thresholdPercent);
    }
}
