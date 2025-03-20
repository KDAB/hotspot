#include "tracepointformat.h"

#include <QLoggingCategory>

extern "C" {
#include <fmt_parser.h>
#include <fmt_util.h>
}

namespace {
Q_LOGGING_CATEGORY(FormatParser, "hotspot.formatparser");

auto formatUnsignedNumber(const FormatConversion& format, int base, const QVariant& value)
{
    switch (format.len) {
    case FormatConversion::Length::Char:
        return QStringLiteral("%1").arg(static_cast<unsigned char>(value.toULongLong()), format.width, base,
                                        QLatin1Char('0'));
    case FormatConversion::Length::Short:
        return QStringLiteral("%1").arg(static_cast<unsigned short>(value.toULongLong()), format.width, base,
                                        QLatin1Char('0'));
    case FormatConversion::Length::Long:
        return QStringLiteral("%1").arg(static_cast<unsigned int>(value.toULongLong()), format.width, base,
                                        QLatin1Char('0'));
    case FormatConversion::Length::Size:
    case FormatConversion::Length::LongLong:
        return QStringLiteral("%1").arg(value.toULongLong(), format.width, base, QLatin1Char('0'));
    }
    Q_UNREACHABLE();
}

auto formatSignedNumber(const FormatConversion& format, int base, const QVariant& value)
{
    switch (format.len) {
    case FormatConversion::Length::Char:
        return QStringLiteral("%1").arg(static_cast<char>(value.toLongLong()), format.width, base, QLatin1Char('0'));
    case FormatConversion::Length::Short:
        return QStringLiteral("%1").arg(static_cast<short>(value.toLongLong()), format.width, base, QLatin1Char('0'));
    case FormatConversion::Length::Long:
        return QStringLiteral("%1").arg(static_cast<int>(value.toLongLong()), format.width, base, QLatin1Char('0'));
    case FormatConversion::Length::Size:
    case FormatConversion::Length::LongLong:
        return QStringLiteral("%1").arg(value.toLongLong(), format.width, base, QLatin1Char('0'));
    }
    Q_UNREACHABLE();
}
}

FormatData parseFormatString(const QString& format)
{
    // try to parse the format string
    // if it fails or we encounter unknown flags bail out
    auto latin = format.toLatin1().toStdString();

    const char* str = latin.c_str();

    fmt_status rc;
    fmt_spec spec;

    QVector<FormatConversion> formats;
    QVector<QByteArray> qtFormatString;
    int formatCounter = 1;

    do {
        fmt_spec_init(&spec);
        rc = fmt_read_one(&str, &spec);
        if (rc == FMT_EOK) {
            fmt_spec_print(&spec, stdout);
            printf("\n");
            FormatConversion format;

            if (spec.kind == FMT_SPEC_KIND_STRING) {
                qtFormatString.append(QByteArray {spec.str_start, static_cast<int>(spec.str_end - spec.str_start)});
            } else {
                qtFormatString.append(QStringLiteral("%%1").arg(formatCounter++).toLatin1());

                switch (static_cast<fmt_spec_len>(spec.len)) {
                case FMT_SPEC_LEN_hh:
                    format.len = FormatConversion::Length::Char;
                    break;
                case FMT_SPEC_LEN_h:
                    format.len = FormatConversion::Length::Short;
                    break;
                case FMT_SPEC_LEN_L:
                case FMT_SPEC_LEN_l:
                    format.len = FormatConversion::Length::Long;
                    break;
                case FMT_SPEC_LEN_ll:
                    format.len = FormatConversion::Length::LongLong;
                    break;
                case FMT_SPEC_LEN_z:
                    format.len = FormatConversion::Length::Size;
                    break;
                case FMT_SPEC_LEN_UNKNOWN:
                    // no length given
                    break;
                default:
                    qCWarning(FormatParser) << "Failed to parse fmt_spec_len" << spec.len;
                    return {};
                }

                switch (static_cast<fmt_spec_type>(spec.type)) {
                case FMT_SPEC_TYPE_X:
                    format.format = FormatConversion::Format::UpperHex;
                    break;
                case FMT_SPEC_TYPE_x:
                    format.format = FormatConversion::Format::Hex;
                    break;
                case FMT_SPEC_TYPE_o:
                    format.format = FormatConversion::Format::Octal;
                    break;
                case FMT_SPEC_TYPE_d:
                case FMT_SPEC_TYPE_i:
                    format.format = FormatConversion::Format::Signed;
                    break;
                case FMT_SPEC_TYPE_u:
                    format.format = FormatConversion::Format::Unsigned;
                    break;
                case FMT_SPEC_TYPE_c:
                    format.format = FormatConversion::Format::Char;
                    break;
                case FMT_SPEC_TYPE_p:
                    format.format = FormatConversion::Format::Pointer;
                    break;
                case FMT_SPEC_TYPE_s:
                    format.format = FormatConversion::Format::String;
                    break;
                default:
                    qCWarning(FormatParser) << "Failed to parse fmt_spec_type" << spec.type;
                    return {};
                }

                if (spec.flags.prepend_zero) {
                    format.padZeros = true;
                    format.width = spec.width;

                    if (spec.width == FMT_VALUE_OUT_OF_LINE) {
                        return {};
                    }
                }

                formats.push_back(format);
            }
        }
    } while (fmt_read_is_ok(rc));

    return {formats, QString::fromLatin1(qtFormatString.join())};
}

QString format(const FormatConversion& format, const QVariant& value)
{
    switch (format.format) {
    case FormatConversion::Format::Signed:
        return formatSignedNumber(format, 10, value);
    case FormatConversion::Format::Unsigned:
        return formatUnsignedNumber(format, 10, value);
    case FormatConversion::Format::Char:
        return value.toChar();
    case FormatConversion::Format::String:
        return value.toString();
    case FormatConversion::Format::Pointer:
        return QStringLiteral("0x%1").arg(QString::number(value.toULongLong(), 16));
    case FormatConversion::Format::Hex:
        return formatUnsignedNumber(format, 16, value);
    case FormatConversion::Format::UpperHex:
        return formatUnsignedNumber(format, 16, value).toUpper();
    case FormatConversion::Format::Octal:
        return formatUnsignedNumber(format, 8, value);
    }
    return {};
}

TracePointFormatter::TracePointFormatter(const QString& format)
{
    // ignore empty format strings
    if (format.isEmpty()) {
        return;
    }

    // the format string are the arguments to a printf call, therefor the format will always be in quotes and then
    // follows a list of arguments
    auto endOfFormatString = format.indexOf(QLatin1Char('\"'), 1);

    auto formatStringExtracted = format.mid(1, endOfFormatString - 1);
    auto formats = parseFormatString(formatStringExtracted);

    const auto args = format.mid(endOfFormatString + 2).split(QLatin1Char(','));

    // we successfully parsed the string
    if (formats.format.size() == args.size()) {
        m_formatString = formats.formatString;
        for (int i = 0; i < formats.format.size(); i++) {
            const auto& arg = args[i];
            auto rec = arg.indexOf(QLatin1String("REC->"));

            if (rec == -1) {
                return;
            }

            auto closingBracket = arg.indexOf(QLatin1Char(')'), rec);
            if (closingBracket == -1) {
                return;
            }
            // TODO: safeguard this
            rec += 5;
            m_args.push_back({formats.format[i], arg.mid(rec, closingBracket - rec)});
        }
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

    result = m_formatString;
    for (const auto& arg : m_args) {
        result = result.arg(::format(arg.format, data.value(arg.name)));
    }
    return result;
}

QString formatTracepoint(const Data::TracePointFormat& format, const Data::TracePointData& data)
{
    static QHash<QString, TracePointFormatter> formatterCache;

    const auto name = QStringLiteral("%1:%2").arg(format.systemId, format.nameId);
    auto formatter = formatterCache.find(name);
    if (formatter == formatterCache.end()) {
        formatter = formatterCache.emplace(name, format.format);
    }

    return QStringLiteral("%1:\n%2").arg(name, formatter->format(data));
}
