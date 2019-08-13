/*
  util.cpp

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

#include "util.h"

#include "hotspot-config.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

#include <initializer_list>

#include "data.h"
#include "settings.h"

QString Util::findLibexecBinary(const QString& name)
{
    QDir dir(qApp->applicationDirPath());
    if (!dir.cd(QStringLiteral(HOTSPOT_LIBEXEC_REL_PATH))) {
        return {};
    }
    QFileInfo info(dir.filePath(name));
    if (!info.exists() || !info.isFile() || !info.isExecutable()) {
        return {};
    }
    return info.absoluteFilePath();
}

QString Util::formatString(const QString& input, bool replaceEmptyString)
{
    return input.isEmpty() && replaceEmptyString ? QCoreApplication::translate("Util", "??") : input;
}

QString Util::formatSymbol(const Data::Symbol& symbol, bool replaceEmptyString)
{
    return formatString(Settings::instance()->prettifySymbols() ? symbol.prettySymbol : symbol.symbol,
                        replaceEmptyString);
}

QString Util::formatCost(quint64 cost)
{
    // resulting format: 1.234E56
    return QString::number(static_cast<double>(cost), 'G', 4);
}

QString Util::formatCostRelative(quint64 selfCost, quint64 totalCost, bool addPercentSign)
{
    if (!totalCost) {
        return QString();
    }

    auto ret = QString::number(static_cast<double>(selfCost) * 100. / totalCost, 'G', 3);
    if (addPercentSign) {
        ret.append(QLatin1Char('%'));
    }
    return ret;
}

QString Util::formatTimeString(quint64 nanoseconds, bool shortForm)
{
    if (nanoseconds < 1000) {
        return QString::number(nanoseconds) + QLatin1String("ns");
    }

    auto format = [](quint64 fragment, int precision) -> QString {
        return QString::number(fragment).rightJustified(precision, QLatin1Char('0'));
    };

    quint64 microseconds = nanoseconds / 1000;
    if (nanoseconds < 1000000) {
        quint64 nanos = nanoseconds % 1000;
        if (shortForm) {
            return QString::number(microseconds) + QStringLiteral("µs");
        }
        return format(microseconds, 3) + QLatin1Char('.') + format(nanos, 3) + QStringLiteral("µs");
    }

    quint64 milliseconds = (nanoseconds / 1000000) % 1000;
    if (nanoseconds < 1000000000) {
        if (shortForm) {
            return QString::number(milliseconds) + QLatin1String("ms");
        }
        return format(milliseconds, 3) + QLatin1Char('.') + format(microseconds, 3) + QLatin1String("ms");
    }

    quint64 totalSeconds = nanoseconds / 1000000000;
    quint64 days = totalSeconds / 60 / 60 / 24;
    quint64 hours = (totalSeconds / 60 / 60) % 24;
    quint64 minutes = (totalSeconds / 60) % 60;
    quint64 seconds = totalSeconds % 60;

    auto optional = [](quint64 fragment, const char *unit) -> QString {
        if (fragment > 0)
            return QString::number(fragment) + QLatin1String(unit) + QLatin1Char(' ');
        return QString();
    };
    if (shortForm) {
        return optional(days, "d") + optional(hours, "h") + optional(minutes, "min") + QString::number(seconds) + QLatin1Char('s');
    }
    return optional(days, "d") + optional(hours, "h") + optional(minutes, "min") + format(seconds, 2) + QLatin1Char('.')
        + format(milliseconds, 3) + QLatin1Char('s');
}

QString Util::formatFrequency(quint64 occurrences, quint64 nanoseconds)
{
    auto hz = 1E9 * occurrences / nanoseconds;

    static const auto units = {"Hz", "KHz", "MHz", "GHz", "THz"};
    auto unit = units.begin();
    auto lastUnit = units.end() - 1;
    while (unit != lastUnit && hz > 1000.) {
        hz /= 1000.;
        ++unit;
    }
    return QString::number(hz, 'G', 4) + QLatin1String(*unit);
}

static QString formatTooltipImpl(int id, const Data::Symbol& symbol, const Data::Costs* selfCosts,
                                 const Data::Costs* inclusiveCosts)
{
    Q_ASSERT(selfCosts || inclusiveCosts);
    Q_ASSERT(!selfCosts || !inclusiveCosts || (selfCosts->numTypes() == inclusiveCosts->numTypes()));

    QString toolTip = QCoreApplication::translate("Util", "symbol: <tt>%1</tt><br/>binary: <tt>%2</tt>")
                          .arg(Util::formatSymbol(symbol).toHtmlEscaped(), Util::formatString(symbol.binary));

    auto extendTooltip = [&toolTip, id](int i, const Data::Costs& costs, const QString& formatting) {
        const auto currentCost = costs.cost(i, id);
        const auto totalCost = costs.totalCost(i);
        toolTip += formatting.arg(costs.typeName(i), costs.formatCost(i, currentCost), costs.formatCost(i, totalCost),
                                  Util::formatCostRelative(currentCost, totalCost));
    };

    const auto numTypes = selfCosts ? selfCosts->numTypes() : inclusiveCosts->numTypes();
    for (int i = 0; i < numTypes; ++i) {
        if (!inclusiveCosts->totalCost(i)) {
            continue;
        }

        toolTip += QLatin1String("<hr/>");
        if (selfCosts) {
            extendTooltip(i, *selfCosts,
                          QCoreApplication::translate("Util", "%1 (self): %2<br/>&nbsp;&nbsp;%4% out of %3 total"));
        }
        if (selfCosts && inclusiveCosts) {
            toolTip += QLatin1String("<br/>");
        }
        if (inclusiveCosts) {
            extendTooltip(
                i, *inclusiveCosts,
                QCoreApplication::translate("Util", "%1 (inclusive): %2<br/>&nbsp;&nbsp;%4% out of %3 total"));
        }
    }
    return QString(QLatin1String("<qt>") + toolTip + QLatin1String("</qt>"));
}

QString Util::formatTooltip(int id, const Data::Symbol& symbol, const Data::Costs& costs)
{
    return formatTooltipImpl(id, symbol, nullptr, &costs);
}

QString Util::formatTooltip(int id, const Data::Symbol& symbol, const Data::Costs& selfCosts,
                            const Data::Costs& inclusiveCosts)
{
    return formatTooltipImpl(id, symbol, &selfCosts, &inclusiveCosts);
}

QString Util::formatTooltip(const Data::Symbol& symbol, const Data::ItemCost& itemCost, const Data::Costs& totalCosts)
{
    Q_ASSERT(static_cast<quint32>(totalCosts.numTypes()) == itemCost.size());
    auto toolTip = QCoreApplication::translate("Util", "symbol: <tt>%1</tt><br/>binary: <tt>%2</tt>")
                       .arg(Util::formatSymbol(symbol), Util::formatString(symbol.binary));
    for (int i = 0, c = totalCosts.numTypes(); i < c; ++i) {
        const auto cost = itemCost[i];
        const auto total = totalCosts.totalCost(i);
        if (!total) {
            continue;
        }
        toolTip += QLatin1String("<hr/>")
            + QCoreApplication::translate("Util", "%1: %2<br/>&nbsp;&nbsp;%4% out of %3 total")
                  .arg(totalCosts.typeName(i), totalCosts.formatCost(i, cost), totalCosts.formatCost(i, total),
                       Util::formatCostRelative(cost, total));
    }
    return QString(QLatin1String("<qt>") + toolTip + QLatin1String("</qt>"));
}

QString Util::formatTooltip(const QString& location, const Data::LocationCost& cost, const Data::Costs& totalCosts)
{
    QString toolTip = location;

    Q_ASSERT(static_cast<quint32>(totalCosts.numTypes()) == cost.inclusiveCost.size());
    Q_ASSERT(static_cast<quint32>(totalCosts.numTypes()) == cost.selfCost.size());
    for (int i = 0, c = totalCosts.numTypes(); i < c; ++i) {
        const auto selfCost = cost.selfCost[i];
        const auto inclusiveCost = cost.inclusiveCost[i];
        const auto total = totalCosts.totalCost(i);
        if (!total) {
            continue;
        }
        toolTip += QLatin1String("<hr/>")
            + QCoreApplication::translate("Util", "%1 (self): %2<br/>&nbsp;&nbsp;%4% out of %3 total")
                  .arg(totalCosts.typeName(i), totalCosts.formatCost(i, selfCost), totalCosts.formatCost(i, total),
                       Util::formatCostRelative(selfCost, total))
            + QLatin1String("<br/>")
            + QCoreApplication::translate("Util", "%1 (inclusive): %2<br/>&nbsp;&nbsp;%4% out of %3 total")
                  .arg(totalCosts.typeName(i), totalCosts.formatCost(i, inclusiveCost), totalCosts.formatCost(i, total),
                       Util::formatCostRelative(inclusiveCost, total));
    }
    return QString(QLatin1String("<qt>") + toolTip + QLatin1String("</qt>"));
}

QProcessEnvironment Util::appImageEnvironment()
{
    static const auto env = QProcessEnvironment::systemEnvironment();
    return env;
}
