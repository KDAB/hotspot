/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "highlightedtext.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPalette>
#include <QTextLayout>

#include <memory>

#include <KColorScheme>

#include <hotspot-config.h>

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

using LineFormat = QVector<QTextLayout::FormatRange>;

#if KFSyntaxHighlighting_FOUND
// highlighter using KSyntaxHighlighting
class HighlightingImplementation : public KSyntaxHighlighting::AbstractHighlighter
{
public:
    HighlightingImplementation(KSyntaxHighlighting::Repository* repository)
        : m_repository(repository)
    {
    }
    ~HighlightingImplementation() override = default;

    void formatText(const QStringList& text)
    {
        if (m_lines != text) {
            m_lines = text;
        }

        m_formats.clear();
        m_state = {};

        for (const auto& line : text) {
            m_formats.push_back(formatLine(line));
        }
    }

    virtual LineFormat format(int lineIndex) const
    {
        return m_formats.at(lineIndex);
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
        formatText(m_lines);
    }

    virtual QString definitionName() const
    {
        return definition().name();
    }

    virtual LineFormat formatLine(const QString& line)
    {
        m_lineFormat.clear();
        m_state = highlightLine(line, m_state);
        return m_lineFormat;
    }

protected:
    void applyFormat(int offset, int length, const KSyntaxHighlighting::Format& format) override
    {
        QTextCharFormat textCharFormat;
        textCharFormat.setForeground(format.textColor(theme()));
        textCharFormat.setFontWeight(format.isBold(theme()) ? QFont::Bold : QFont::Normal);
        m_lineFormat.push_back({offset, length, textCharFormat});
    }

private:
    KSyntaxHighlighting::Repository* m_repository;
    KSyntaxHighlighting::State m_state;
    QStringList m_lines; // for reformatting if definition changes
    LineFormat m_lineFormat;
    QVector<LineFormat> m_formats;
};
#else
// stub in case KSyntaxHighlighting is not available
class HighlightingImplementation
{
public:
    HighlightingImplementation(KSyntaxHighlighting::Repository* /*repository*/) { }
    virtual ~HighlightingImplementation() = default;

    void formatText(const QStringList& text)
    {
        m_formats.clear();

        for (const auto& line : text) {
            m_formats.push_back(formatLine(line));
        }
    }

    LineFormat format(int lineIndex) const
    {
        return m_formats.at(lineIndex);
    }

    virtual void themeChanged() { }

    virtual void setHighlightingDefinition(const KSyntaxHighlighting::Definition& /*definition*/) { }
    virtual QString definitionName() const
    {
        return {};
    };

private:
    // stub implementation necessary for testing
    virtual LineFormat formatLine(const QString& line)
    {
        return {{QTextLayout::FormatRange {0, line.length(), {}}}};
    }

    Q_DISABLE_COPY(HighlightingImplementation)
    QVector<LineFormat> m_formats;
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
    LineFormat formatLine(const QString& text) override
    {
        QVector<QTextLayout::FormatRange> formats;

        int offset = 0;
        QTextLayout::FormatRange format;

        constexpr int setColorSequenceLength = 5;
        constexpr int resetColorSequenceLength = 4;
        constexpr int colorCodeLength = 2;

        auto lastToken = text.begin();
        for (auto escapeIt = std::find(text.cbegin(), text.cend(), Util::escapeChar); escapeIt != text.cend();
             escapeIt = std::find(escapeIt, text.cend(), Util::escapeChar)) {

            Q_ASSERT(*(escapeIt + 1) == QLatin1Char('['));

            offset += std::distance(lastToken, escapeIt);

            // escapeIt + 2 points to the first color code character
            auto color = QStringView {escapeIt + 2, colorCodeLength};
            bool ok = false;
            const uint8_t colorCode = color.toUInt(&ok);
            if (ok) {
                // only support the 8 default colors
                Q_ASSERT(colorCode >= 30 && colorCode <= 37);

                format.start = offset;
                const auto colorRole = static_cast<KColorScheme::ForegroundRole>(colorCode - 30);
                format.format.setForeground(m_colorScheme.foreground(colorRole));

                std::advance(escapeIt, setColorSequenceLength);
            } else {
                // make sure we have a reset sequence
                Q_ASSERT(color == QStringLiteral("0m"));
                format.length = offset - format.start;
                if (format.length) {
                    formats.push_back(format);
                }

                std::advance(escapeIt, resetColorSequenceLength);
            }

            lastToken = escapeIt;
        }

        return formats;
    }

    KColorScheme m_colorScheme;
};

// QTextLayout is slow, this class acts as a cache that only creates and fills the QTextLayout on demand
class HighlightedLine
{
public:
    HighlightedLine() = default;
    HighlightedLine(HighlightingImplementation* highlighter, const QString& text, int index)
        : m_highlighter(highlighter)
        , m_text(Util::removeAnsi(text))
        , m_index(index)
        , m_layout(nullptr)
    {
    }

    QTextLayout* layout() const
    {
        if (!m_layout) {
            m_layout = buildLayout();
        }
        return m_layout.get();
    }

    void updateHighlighting()
    {
        m_layout = nullptr;
    }

    void setTabWidthInPixels(int tabWidthInPixels)
    {
        m_tabWidthInPixels = tabWidthInPixels;
        m_layout = nullptr;
    }

private:
    std::unique_ptr<QTextLayout> buildLayout() const
    {
        Q_ASSERT(m_index != -1);
        auto layout = std::make_unique<QTextLayout>();

        auto option = layout->textOption();
        option.setTabStopDistance(m_tabWidthInPixels);
        layout->setTextOption(option);

        auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        layout->setFont(font);
        layout->setText(m_text);
        layout->setFormats(m_highlighter->format(m_index));

        layout->beginLayout();

        // there is at most one line, so we don't need to check this multiple times
        auto line = layout->createLine();
        if (line.isValid()) {
            line.setPosition(QPointF(0, 0));
        }

        layout->endLayout();

        return layout;
    }

    HighlightingImplementation* m_highlighter = nullptr;
    QString m_text;
    int m_index = -1;
    int m_tabWidthInPixels = -1; // qts default value
    mutable std::unique_ptr<QTextLayout> m_layout;
};
static_assert(std::is_nothrow_move_constructible_v<HighlightedLine>);
static_assert(std::is_nothrow_destructible_v<HighlightedLine>);

HighlightedText::HighlightedText(KSyntaxHighlighting::Repository* repository, QObject* parent)
    : QObject(parent)
    , m_repository(repository)
{
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
        m_highlighter->themeChanged();
        m_isUsingAnsi = usesAnsi;
        emit usesAnsiChanged(usesAnsi);
    }

    m_highlightedLines.resize(text.size());
    m_highlighter->formatText(text);
    int index = 0;
    std::transform(text.cbegin(), text.cend(), m_highlightedLines.begin(), [this, &index](const QString& text) {
        return HighlightedLine {m_highlighter.get(), text, index++};
    });

    // this is free since we currently have no text rendered
    updateTabWidth(m_tabWidth);

    m_cleanedLines = text;
    std::transform(m_cleanedLines.begin(), m_cleanedLines.end(), m_cleanedLines.begin(), Util::removeAnsi);
}

void HighlightedText::setDefinition(const KSyntaxHighlighting::Definition& definition)
{
    Q_ASSERT(m_highlighter);
    m_highlighter->setHighlightingDefinition(definition);
#if KFSyntaxHighlighting_FOUND
    emit definitionChanged(definition.name());
    updateHighlighting();
#endif
}

QString HighlightedText::textAt(int index) const
{
    Q_ASSERT(m_highlighter);
    return m_cleanedLines.at(index);
}

QTextLine HighlightedText::lineAt(int index) const
{
    auto& line = m_highlightedLines[index];
    return line.layout()->lineAt(0);
}

QString HighlightedText::definition() const
{
    if (!m_highlighter)
        return {};
    return m_highlighter->definitionName();
}

QTextLayout* HighlightedText::layoutForLine(int index)
{
    return m_highlightedLines[index].layout();
}

void HighlightedText::updateHighlighting()
{
    if (m_highlighter)
        m_highlighter->themeChanged();
    std::for_each(m_highlightedLines.begin(), m_highlightedLines.end(),
                  [](HighlightedLine& line) { line.updateHighlighting(); });
}

void HighlightedText::updateTabWidth(int tabWidth)
{
    m_tabWidth = tabWidth;
    auto font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    const auto tabWidthInPixels = tabWidth * QFontMetrics(font).horizontalAdvance(QLatin1Char(' '));

    std::for_each(m_highlightedLines.begin(), m_highlightedLines.end(),
                  [tabWidthInPixels](HighlightedLine& line) { line.setTabWidthInPixels(tabWidthInPixels); });
}
