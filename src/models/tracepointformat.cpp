#include "tracepointformat.h"

TracePointFormatter::TracePointFormatter(const QString& format)
{
    // ignore empty format strings
    if (format.isEmpty()) {
        return;
    }

    // the format string are the arguments to a printf call, therefor the format will always be in quotes and then
    // follows a list of arguments
    auto endOfFormatString = format.indexOf(QLatin1Char('\"'), 1);

    auto lastRec = endOfFormatString;
    auto recIndex = lastRec;

    // no quote in format string -> format string is not a string
    if (endOfFormatString == -1) {
        return;
    }

    // check for valid format string
    // some format strings contains this tries to filter these out
    for (int i = endOfFormatString; i < format.size(); i++) {
        auto c = format[i];
        auto nextC = i < format.size() - 1 ? format[i + 1] : QChar {};

        if ((c == QLatin1Char('>') && nextC == QLatin1Char('>'))
            || (c == QLatin1Char('<') && nextC == QLatin1Char('<'))) {
            return;
        }
    }

    // set format string after validating we can print it
    m_formatString = format.mid(1, endOfFormatString - 1);

    while ((recIndex = format.indexOf(QStringLiteral("REC->"), lastRec)) != -1) {
        auto endOfName = format.indexOf(QLatin1Char(')'), recIndex);

        auto start = recIndex + 5; // 5 because we want the field after REC->
        m_args.push_back(format.mid(start, endOfName - start));
        lastRec = recIndex + 1;
    }
}

QString TracePointFormatter::format(const Data::TracePointData& data) const
{
    QString result;

    // if m_formatString is empty, we couldn't parse it, just dump out the information
    if (m_formatString.isEmpty()) {
        for (auto it = data.cbegin(), end = data.cend(); it != end; it++) {
            result += QLatin1String("%1: %2\n").arg(it.key(), QString::number(it->toULongLong()));
        }
        return result.trimmed();
    }

    const auto percent = QLatin1Char('%');
    auto currentPercent = 0;

    for (int i = 0; i < m_args.size();) {
        auto nextPercent = m_formatString.indexOf(percent, currentPercent + 1);

        auto substring = m_formatString.mid(currentPercent, nextPercent - currentPercent);
        if (substring.contains(percent)) {
            result += QString::asprintf(qPrintable(substring), data.value(m_args[i]).toULongLong());
            i++;

            currentPercent = nextPercent;
        } else {
            result += substring;
        }
        currentPercent = nextPercent;
    }

    return result;
}

QString formatTracepoint(const Data::TracePointFormat& format, const Data::TracePointData& data)
{
    TracePointFormatter formatter(format.format);
    return QStringLiteral("%1:%2:\n%3").arg(format.systemId, format.nameId, formatter.format(data));
}
