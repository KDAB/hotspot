/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "data.h"
#include "models/callercalleemodel.h"

class QTextStream;
class QModelIndex;

enum class Direction
{
    Caller,
    Callee
};

QHash<Data::Symbol, QString> writeGraph(QTextStream& stream, const Data::Symbol& symbol,
                                        Data::CallerCalleeResults& results, float thresholdPercent,
                                        const QString& fontColor);
void resultsToDot(int height, Direction direction, const Data::Symbol& symbol, Data::CallerCalleeResults& results,
                  const QString& parent, QTextStream& stream, QHash<Data::Symbol, QString>& nodeIdLookup,
                  float thresholdPercent);
