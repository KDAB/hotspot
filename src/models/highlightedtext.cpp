/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "highlightedtext.h"

#include <QGuiApplication>
#include <QPalette>

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

    QVector<QTextLayout::FormatRange> format(const QStringList& text)
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

    void themeChanged()
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

    void setHighlightingDefinition(const KSyntaxHighlighting::Definition& definition)
    {
        setDefinition(definition);
    }

    QString definitionName() const
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
    HighlightingImplementation(KSyntaxHighlighting::Repository*) = default;
    ~HighlightingImplementation() override = default;

    QVector<QTextLayout::FormatRange> format(const QStringList& text) override
    {
        return {};
    }

    void themeChanged() override { }

    void setHighlightingDefinition(const KSyntaxHighlighting::Definition& /*definition*/) override { }
    QString definitionName() const override
    {
        return {};
    };
#endif

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
    if (!m_highlighter) {
        m_highlighter = std::make_unique<HighlightingImplementation>(m_repository);
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
