/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "util.h"

#include "hotspot-config.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <initializer_list>

#include "data.h"
#include "settings.h"

#include <kcoreaddons_version.h>

#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 86, 0)
#include <KPluginFactory>
#include <KPluginMetaData>
#else
#include <KService>
#endif

#include <KParts/ReadOnlyPart>

namespace {
std::pair<QString, QString> elideArguments(const QString& symbolText)
{
    QChar quoteChar = QLatin1Char('"');
    int numQuotes = 0;
    int depth = 0;

    int endEliding = symbolText.length();
    int startEliding = 0;

    // this loop tries to to find the opening and closing bracket of the arguments
    // it does this by counting the number of opening and closing brackets from the back
    // it also has some basic string checking, so that string arguments like "(" of "\')"
    // will not be counted
    for (int i = symbolText.length() - 1; i > 0; i--) {
        auto c = symbolText[i];

        // detect quotes and skip those
        if ((c == QLatin1Char('"') || c == QLatin1Char('\'')) && numQuotes == 0) {
            quoteChar = c;
            numQuotes++;
        } else if (c == quoteChar && numQuotes == 1) {
            if (symbolText[i - 1] == QLatin1Char('\\')) {
                continue;
            }
            numQuotes = 0;
            continue;
        }
        if (numQuotes == 1)
            continue;

        // mark opening and closing brackets
        if (c == QLatin1Char(')')) {
            if (depth == 0) {
                endEliding = i;
            }
            depth++;
        }
        if (c == QLatin1Char('(')) {
            depth--;
            if (depth == 0) {
                startEliding = i;
                break;
            }
        }
    }

    if (endEliding - startEliding == 1) {
        // don't elide if we have () in symbol
        return {symbolText, {}};
    }

    return {symbolText.left(startEliding + 1), symbolText.right(symbolText.length() - endEliding)};
}

QString formatForTooltip(const Data::Symbol& symbol)
{
    return QCoreApplication::translate("Util", "symbol: <tt>%1</tt><br/>binary: <tt>%2</tt>")
        .arg(Util::formatSymbolExtended(symbol).toHtmlEscaped(), Util::formatString(symbol.binary));
}

QString formatTooltipImpl(int id, const QString& text, const Data::Costs* selfCosts, const Data::Costs* inclusiveCosts)
{
    Q_ASSERT(selfCosts || inclusiveCosts);
    Q_ASSERT(!selfCosts || !inclusiveCosts || (selfCosts->numTypes() == inclusiveCosts->numTypes()));

    QString toolTip = text;

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
}

QString Util::collapseTemplate(const QString& str, int level)
{
    if (str.indexOf(QLatin1Char('<')) == -1) {
        return str;
    }

    // special handling for fake section symbols of the form <.SECTION+OFFSET>
    if (str.startsWith(QLatin1String("<.")) && str.endsWith(QLatin1Char('>'))) {
        return str;
    }

    auto isSpaceOrAngleBracket = [](QChar c) {
        if (c == QLatin1Char(' ')) {
            return true;
        }
        if (c == QLatin1Char('<')) {
            return true;
        }
        if (c == QLatin1Char('>')) {
            return true;
        }

        return false;
    };

    // QLatin1String does not work with …
    const auto elideString = QStringLiteral("<…");

    QString output;
    output.reserve(str.size());
    const auto operatorKeyword = QLatin1String("operator");
    const int size = str.size();
    int depth = 0;
    for (int i = 0; i < size; i++) {
        const auto c = str[i];
        if (c == QLatin1Char('<')) {
            depth++;
            if (depth == level) {
                output.append(elideString);
            }
        } else if (c == QLatin1Char('>')) {
            depth--;
        } else if (c == QLatin1Char('o') && QStringView(str).mid(i, operatorKeyword.size()) == operatorKeyword) {
            i += operatorKeyword.size();
            output.append(operatorKeyword);
            int j = i;
            for (; j < size && isSpaceOrAngleBracket(str[j]); j++) {
                output.append(str[j]);
            }
            // str[j] now points to a char that is not a space or angle bracket
            // -> need to rewind back
            i = j - 1;
            continue;
        }

        if (depth < level) {
            output.append(c);
        }
    }

    return output;
}

QString Util::elideSymbol(const QString& symbolText, const QFontMetrics& metrics, int maxWidth)
{
    if (metrics.horizontalAdvance(symbolText) < maxWidth) {
        return symbolText;
    }

    const auto argumentsElided = elideArguments(symbolText);
    const auto fixedWidthLength =
        metrics.horizontalAdvance(argumentsElided.first) + metrics.horizontalAdvance(argumentsElided.second);

    QString elided;

    if (fixedWidthLength < maxWidth) {
        auto elidedArguments =
            symbolText.mid(argumentsElided.first.size(), symbolText.size() - argumentsElided.second.size());

        elided = argumentsElided.first
            + metrics.elidedText(elidedArguments, Qt::TextElideMode::ElideRight, maxWidth - fixedWidthLength)
            + argumentsElided.second;
        return elided;
    } else {
        elided = argumentsElided.first + QStringLiteral("…") + argumentsElided.second;
    }

    auto templateElided = collapseTemplate(elided, 1);
    if (metrics.horizontalAdvance(templateElided) < maxWidth) {
        return templateElided;
    }

    return metrics.elidedText(templateElided, Qt::TextElideMode::ElideLeft, maxWidth);
}

QString Util::findLibexecBinary(const QString& name)
{
    QDir dir(qApp->applicationDirPath());
    if (!dir.cd(QStringLiteral(HOTSPOT_LIBEXEC_REL_PATH))) {
        return {};
    }
    const auto info = QFileInfo(dir.filePath(name));
    if (!info.exists() || !info.isFile() || !info.isExecutable()) {
        return {};
    }
    return info.absoluteFilePath();
}

QString Util::perfParserBinaryPath()
{
    auto parserBinary = QString::fromLocal8Bit(qgetenv("HOTSPOT_PERFPARSER"));
    if (parserBinary.isEmpty()) {
        parserBinary = Util::findLibexecBinary(QStringLiteral("hotspot-perfparser"));
    } else {
        parserBinary = QStandardPaths::findExecutable(parserBinary);
    }
    return parserBinary;
}

QString Util::formatString(const QString& input, bool replaceEmptyString)
{
    return input.isEmpty() && replaceEmptyString ? QCoreApplication::translate("Util", "??") : input;
}

QString Util::formatSymbol(const Data::Symbol& symbol, bool replaceEmptyString)
{
    QString symbolString = Settings::instance()->prettifySymbols() ? symbol.prettySymbol : symbol.symbol;
    if (Settings::instance()->collapseTemplates()) {
        symbolString = collapseTemplate(symbolString, Settings::instance()->collapseDepth());
    }

    return formatString(symbolString, replaceEmptyString);
}

QString Util::formatSymbolExtended(const Data::Symbol& symbol)
{
    auto ret = formatSymbol(symbol);
    if (symbol.isInline) {
        ret = QCoreApplication::translate("Util", "%1 (inlined)").arg(ret);
    }
    return ret;
}

QString Util::formatCost(quint64 cost)
{
    // resulting format: 1.234E56
    return QString::number(static_cast<double>(cost), 'G', 4);
}

QString Util::formatCostRelative(quint64 selfCost, quint64 totalCost, bool addPercentSign)
{
    if (!totalCost) {
        return {};
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

    const auto microseconds = nanoseconds / 1000;
    if (nanoseconds < 1000000) {
        const auto nanos = nanoseconds % 1000;
        if (shortForm) {
            return QString::number(microseconds) + QStringLiteral("µs");
        }
        return format(microseconds, 3) + QLatin1Char('.') + format(nanos, 3) + QStringLiteral("µs");
    }

    const auto milliseconds = (nanoseconds / 1000000) % 1000;
    if (nanoseconds < 1000000000) {
        if (shortForm) {
            return QString::number(milliseconds) + QLatin1String("ms");
        }
        return format(milliseconds, 3) + QLatin1Char('.') + format(microseconds, 3) + QLatin1String("ms");
    }

    const auto totalSeconds = nanoseconds / 1000000000;
    const auto days = totalSeconds / 60 / 60 / 24;
    const auto hours = (totalSeconds / 60 / 60) % 24;
    const auto minutes = (totalSeconds / 60) % 60;
    const auto seconds = totalSeconds % 60;

    auto optional = [](quint64 fragment, const char* unit) -> QString {
        if (fragment > 0)
            return QString::number(fragment) + QLatin1String(unit) + QLatin1Char(' ');
        return {};
    };
    if (shortForm) {
        return optional(days, "d") + optional(hours, "h") + optional(minutes, "min") + QString::number(seconds)
            + QLatin1Char('s');
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

QString Util::formatBinaryTooltip(int id, const Data::Symbol& symbol, const Data::Costs& costs)
{
    return formatTooltipImpl(id, Util::formatString(symbol.binary), nullptr, &costs);
}

QString Util::formatTooltip(int id, const Data::Symbol& symbol, const Data::Costs& costs)
{
    return formatTooltipImpl(id, formatForTooltip(symbol), nullptr, &costs);
}

QString Util::formatTooltip(int id, const Data::Symbol& symbol, const Data::Costs& selfCosts,
                            const Data::Costs& inclusiveCosts)
{
    return formatTooltipImpl(id, formatForTooltip(symbol), &selfCosts, &inclusiveCosts);
}

QString Util::formatTooltip(const Data::Symbol& symbol, const Data::ItemCost& itemCost, const Data::Costs& totalCosts)
{
    Q_ASSERT(static_cast<quint32>(totalCosts.numTypes()) == itemCost.size());
    auto toolTip = formatForTooltip(symbol);
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

QString Util::formatTooltip(const Data::FileLine& fileLine, const Data::Costs& selfCosts,
                            const Data::Costs& inclusiveCosts)
{
    return formatTooltipImpl(fileLine.line, fileLine.toString(), &selfCosts, &inclusiveCosts);
}

QString Util::formatTooltip(const Data::FileLine& fileLine, const Data::LocationCost& cost,
                            const Data::Costs& totalCosts)
{
    return Util::formatTooltip(fileLine.toString(), cost, totalCosts);
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

KParts::ReadOnlyPart* Util::createPart(const QString& pluginName)
{
#if KCOREADDONS_VERSION >= QT_VERSION_CHECK(5, 86, 0)
    const KPluginMetaData md(pluginName);

    const auto result = KPluginFactory::instantiatePlugin<KParts::ReadOnlyPart>(md, nullptr, {});

    return result.plugin;
#else
    KService::Ptr service = KService::serviceByDesktopName(pluginName);

    if (!service) {
        return nullptr;
    }
    return service->createInstance<KParts::ReadOnlyPart>();
#endif
}
