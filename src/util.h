/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <valarray>
#include <QHashFunctions>
#include <QtGlobal>

class QString;
class QProcessEnvironment;
class QFontMetrics;

namespace Data {
struct Symbol;
struct FileLine;
struct LocationCost;
class Costs;
using ItemCost = std::valarray<qint64>;
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

// HashCombine was taken from Qt's file qhashfunctions.h
struct HashCombine
{
    typedef uint result_type;
    template<typename T>
    Q_DECL_CONSTEXPR result_type operator()(uint seed, const T& t) const Q_DECL_NOEXCEPT_EXPR(noexcept(qHash(t)))
    // combiner taken from N3876 / boost::hash_combine
    {
        return seed ^ (qHash(t) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }
};

QString formatString(const QString& input, bool replaceEmptyString = true);
QString formatSymbol(const Data::Symbol& symbol, bool replaceEmptyString = true);
QString formatCost(quint64 cost);
QString formatCostRelative(quint64 selfCost, quint64 totalCost, bool addPercentSign = false);
QString formatTimeString(quint64 nanoseconds, bool shortForm = false);
QString formatFrequency(quint64 occurrences, quint64 nanoseconds);
QString formatTooltip(int id, const Data::Symbol& symbol, const Data::Costs& costs);
QString formatTooltip(int id, const Data::Symbol& symbol, const Data::Costs& selfCosts,
                      const Data::Costs& inclusiveCosts);
QString formatTooltip(const Data::Symbol& symbol, const Data::ItemCost& itemCost, const Data::Costs& totalCosts);
QString formatTooltip(const Data::FileLine& fileLine, const Data::LocationCost& cost, const Data::Costs& totalCosts);
QString formatTooltip(const Data::FileLine& fileLine, const Data::Costs& selfCosts, const Data::Costs& inclusiveCosts);
QString formatTooltip(const QString& location, const Data::LocationCost& cost, const Data::Costs& totalCosts);

QString elideSymbol(const QString& symbolText, const QFontMetrics& metrics, int maxWidth);

// the process environment including the custom AppImage-specific LD_LIBRARY_PATH
// this is initialized on the first call and cached internally afterwards
QProcessEnvironment appImageEnvironment();

KParts::ReadOnlyPart* createPart(const QString& pluginName);
}
