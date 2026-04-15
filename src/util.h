/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QtGlobal>

class QString;
class QProcessEnvironment;
class QFontMetrics;

namespace Data {
struct Symbol;
struct FileLine;
struct LocationCost;
class Costs;
class ItemCost;
}

namespace KParts {
class ReadOnlyPart;
}

namespace Util {

/**
 * Find a binary called @p name in this application's libexec directory.
 */
QString findLibexecBinary(const QString& name);

/**
 * Find the perfparser binary and return its path.
 */
QString perfParserBinaryPath();

QString formatString(const QString& input, bool replaceEmptyString = true);
QString formatSymbol(const Data::Symbol& symbol, bool replaceEmptyString = true);
QString formatSymbolExtended(const Data::Symbol& symbol);
QString formatCost(quint64 cost);
QString formatCostRelative(quint64 selfCost, quint64 totalCost, bool addPercentSign = false);
QString formatTimeString(quint64 nanoseconds, bool shortForm = false);
QString formatFrequency(quint64 occurrences, quint64 nanoseconds);
QString formatBinaryTooltip(int id, const Data::Symbol& symbol, const Data::Costs& costs);
QString formatFileTooltip(int id, const QString& file, const Data::Costs& selfCosts, const Data::Costs& inclusiveCosts);
QString formatTooltip(int id, const Data::Symbol& symbol, const Data::Costs& costs);
QString formatTooltip(int id, const Data::Symbol& symbol, const Data::Costs& selfCosts,
                      const Data::Costs& inclusiveCosts);
QString formatTooltip(const Data::Symbol& symbol, const Data::ItemCost& itemCost, const Data::Costs& totalCosts);
QString formatTooltip(const Data::FileLine& fileLine, const Data::LocationCost& cost, const Data::Costs& totalCosts);
QString formatTooltip(const Data::FileLine& fileLine, const Data::Costs& selfCosts, const Data::Costs& inclusiveCosts);
QString formatTooltip(const QString& location, const Data::LocationCost& cost, const Data::Costs& totalCosts);

QString elideSymbol(const QString& symbolText, const QFontMetrics& metrics, int maxWidth);
QString collapseTemplate(const QString& str, int level);

// the process environment including the custom AppImage-specific LD_LIBRARY_PATH
// this is initialized on the first call and cached internally afterwards
QProcessEnvironment appImageEnvironment();

KParts::ReadOnlyPart* createPart(const QString& pluginName);
}
