/*
  util.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2019 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Milian Wolff <milian.wolff@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <valarray>
#include <QHashFunctions>
#include <QtGlobal>

class QString;
class QProcessEnvironment;

namespace Data {
struct Symbol;
struct LocationCost;
class Costs;
using ItemCost = std::valarray<qint64>;
}

namespace Util {

/**
 * Find a binary called @p name in this application's libexec directory.
 */
QString findLibexecBinary(const QString& name);

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
QString formatTooltip(const QString& location, const Data::LocationCost& cost, const Data::Costs& totalCosts);

// the process environment including the custom AppImage-specific LD_LIBRARY_PATH
// this is initialized on the first call and cached internally afterwards
QProcessEnvironment appImageEnvironment();
}
