/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "highlightedtext.h"

#include <QGuiApplication>
#include <QPalette>
#include <QTextLayout>

#include <memory>

#include <KColorScheme>

#if KFSyntaxHighlighting_FOUND
#include <KSyntaxHighlighting/AbstractHighlighter>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#endif

#include "formattingutils.h"

#if KFSyntaxHighlighting_FOUND
class HighlightingImplementation : public KSyntaxHighlighting::AbstractHighlighter
{
public:
    HighlightingImplementation(KSyntaxHighlighting::Repository* repository)
        : m_repository(repository)
    {
    }
    ~HighlightingImplementation() override = default;

    virtual QVector<QTextLayout::FormatRange> format(const QStringList& text)
    {
        m_formats.clear();
        m_offset = 0;

        KSyntaxHighlighting::State state;
        for (const auto& line : text) {
            state = highlightLine(line, state);

            // KSyntaxHighlighting uses line offsets but QTextLayout uses global offsets
            m_offset += line.size();
        }

        return m_formats;
    }

    virtual void themeChanged()
    {
        if (!m_repository) {
            return;
        }

        KSyntaxHighlighting::Repository::DefaultTheme theme;
        const auto palette = QGuiApplication::palette();
        if (palette.base().color().lightness() < 128) {
            theme = KSyntaxHighlighting::Repository::DarkTheme;
        } else {
            theme = KSyntaxHighlighting::Repository::LightTheme;
        }
        setTheme(m_repository->defaultTheme(theme));
    }

    virtual void setHighlightingDefinition(const KSyntaxHighlighting::Definition& definition)
    {
        setDefinition(definition);
    }

    virtual QString definitionName() const
    {
        return definition().name();
    }

protected:
    void applyFormat(int offset, int length, const KSyntaxHighlighting::Format& format) override
    {
        QTextCharFormat textCharFormat;
        textCharFormat.setForeground(format.textColor(theme()));
        m_formats.push_back({m_offset + offset, length, textCharFormat});
    }

private:
    KSyntaxHighlighting::Repository* m_repository;
    QVector<QTextLayout::FormatRange> m_formats;
    int m_offset = 0;
};
#else
class HighlightingImplementation
{
public:
    virtual HighlightingImplementation(KSyntaxHighlighting::Repository*) = default;
    ~HighlightingImplementation() override = default;

    virtual QVector<QTextLayout::FormatRange> format(const QStringList& text) override
    {
        return {};
    }

    virtual void themeChanged() override { }

    virtual void setHighlightingDefinition(const KSyntaxHighlighting::Definition& /*definition*/) override { }
    virtual QString definitionName() const override
    {
        return {};
    };
#endif

class AnsiHighlightingImplementation : public HighlightingImplementation
{
public:
    AnsiHighlightingImplementation()
        : HighlightingImplementation(nullptr)
    {
    }
    ~AnsiHighlightingImplementation() override = default;

    QVector<QTextLayout::FormatRange> format(const QStringList& text) final
    {
        QVector<QTextLayout::FormatRange> formats;

        int offset = 0;
        QTextLayout::FormatRange format;

        constexpr int setColorSequenceLength = 5;
        constexpr int resetColorSequenceLength = 4;
        constexpr int colorCodeLength = 2;

        for (const auto& line : text) {
            auto lastToken = line.cbegin();
            int lineOffset = 0;
            for (auto escapeIt = std::find(line.cbegin(), line.cend(), Util::escapeChar); escapeIt != line.cend();
                 escapeIt = std::find(escapeIt, line.cend(), Util::escapeChar)) {

                lineOffset += std::distance(lastToken, escapeIt);
                Q_ASSERT(*(escapeIt + 1) == QLatin1Char('['));

                // escapeIt + 2 points to the first color code character
                auto color = QStringView {escapeIt + 2, colorCodeLength};
                bool ok = false;
                const uint8_t colorCode = color.toUInt(&ok);
                if (ok) {
                    // only support the 8 default colors
                    Q_ASSERT(colorCode >= 30 && colorCode <= 37);

                    format.start = offset + lineOffset;
                    const auto colorRole = static_cast<KColorScheme::ForegroundRole>(colorCode - 30);
                    format.format.setForeground(m_colorScheme.foreground(colorRole));

                    std::advance(escapeIt, setColorSequenceLength);
                } else {
                    // make sure we have a reset sequence
                    Q_ASSERT(color == QStringLiteral("0m"));
                    format.length = offset + lineOffset - format.start;
                    if (format.length) {
                        formats.push_back(format);
                    }

                    std::advance(escapeIt, resetColorSequenceLength);
                }
                lastToken = escapeIt;
            }
            offset += lineOffset + std::distance(lastToken, line.cend());
        }

        return formats;
    }

    void themeChanged() override
    {
        m_colorScheme = KColorScheme(QPalette::Normal, KColorScheme::Complementary);
    }

    void setHighlightingDefinition(const KSyntaxHighlighting::Definition& /*definition*/) override { }
    QString definitionName() const override
    {
        return {};
    }

private:
    KColorScheme m_colorScheme;
};

HighlightedText::HighlightedText(KSyntaxHighlighting::Repository* repository, QObject* parent)
    : QObject(parent)
#if KFSyntaxHighlighting_FOUND
    , m_repository(repository)
#endif
    , m_layout(std::make_unique<QTextLayout>())
{
    m_layout->setCacheEnabled(true);
    m_layout->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
}

HighlightedText::~HighlightedText() = default;

void HighlightedText::setText(const QStringList& text)
{
    const bool usesAnsi =
        std::any_of(text.cbegin(), text.cend(), [](const QString& line) { return line.contains(Util::escapeChar); });

    if (!m_highlighter || m_isUsingAnsi != usesAnsi) {
        if (usesAnsi) {
            m_highlighter = std::make_unique<AnsiHighlightingImplementation>();
        } else {
            m_highlighter = std::make_unique<HighlightingImplementation>(m_repository);
        }
        m_isUsingAnsi = usesAnsi;
        emit usesAnsiChanged(usesAnsi);
    }

    m_highlighter->themeChanged();
    m_highlighter->format(text);

    m_lines.reserve(text.size());
    m_cleanedLines.reserve(text.size());

    QString formattedText;

    for (const auto& line : text) {
        const auto& lineWithNewline = QLatin1String("%1%2").arg(line, QChar::LineSeparator);
        const auto& ansiFreeLine = Util::removeAnsi(lineWithNewline);
        m_cleanedLines.push_back(ansiFreeLine);
        m_lines.push_back(lineWithNewline);
        formattedText += ansiFreeLine;
    }

    m_layout->setText(formattedText);

    applyFormatting();
}

void HighlightedText::setDefinition(const KSyntaxHighlighting::Definition& definition)
{
    Q_ASSERT(m_highlighter);
    m_highlighter->setHighlightingDefinition(definition);
    emit definitionChanged(definition.name());
    applyFormatting();
}

QString HighlightedText::textAt(int index) const
{
    Q_ASSERT(m_highlighter);
    Q_ASSERT(index < m_cleanedLines.size());
    return m_cleanedLines.at(index);
}

QTextLine HighlightedText::lineAt(int index) const
{
    Q_ASSERT(m_layout);
    return m_layout->lineAt(index);
}

void HighlightedText::applyFormatting()
{
    Q_ASSERT(m_highlighter);

    m_layout->setFormats(m_highlighter->format(m_lines));

    m_layout->clearLayout();
    m_layout->beginLayout();

    while (true) {
        QTextLine line = m_layout->createLine();
        if (!line.isValid())
            break;

        line.setPosition(QPointF(0, 0));
    }
    m_layout->endLayout();
}

QString HighlightedText::definition() const
{
    if (!m_highlighter)
        return {};
    return m_highlighter->definitionName();
}

QTextLayout* HighlightedText::layout() const
{
    return m_layout.get();
}
