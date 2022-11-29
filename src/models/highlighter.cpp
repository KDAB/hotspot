/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "highlighter.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QTextDocument>

#if KF5SyntaxHighlighting_FOUND
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#endif

Highlighter::Highlighter(QTextDocument* document, QObject* parent)
    : QObject(parent)
#if KF5SyntaxHighlighting_FOUND
    , m_repository(std::make_unique<KSyntaxHighlighting::Repository>())
    , m_highlighter(new KSyntaxHighlighting::SyntaxHighlighter(document))
#endif
{
    document->setDefaultFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    // if qApp is used, UBSAN complains
    QApplication::instance()->installEventFilter(this);

    updateColorTheme();
}

Highlighter::~Highlighter() = default;

void Highlighter::setDefinitionForFilename(const QString& filename)
{
#if KF5SyntaxHighlighting_FOUND
    const auto def = m_repository->definitionForFileName(filename);
    m_highlighter->setDefinition(def);
#else
    Q_UNUSED(filename);
#endif
}

void Highlighter::setDefinitionForName(const QString& name)
{
#if KF5SyntaxHighlighting_FOUND
    const auto def = m_repository->definitionForName(name);
    m_highlighter->setDefinition(def);
#else
    Q_UNUSED(name);
#endif
}

bool Highlighter::eventFilter(QObject* /*watched*/, QEvent* event)
{
    if (event->type() == QEvent::Type::ApplicationPaletteChange) {
        updateColorTheme();
    }

    return false;
}

void Highlighter::updateColorTheme()
{
#if KF5SyntaxHighlighting_FOUND
    KSyntaxHighlighting::Repository::DefaultTheme theme;
    if (QPalette().base().color().lightness() < 128) {
        theme = KSyntaxHighlighting::Repository::DarkTheme;
    } else {
        theme = KSyntaxHighlighting::Repository::LightTheme;
    }
    m_highlighter->setTheme(m_repository->defaultTheme(theme));
    m_highlighter->rehighlight();
#endif
}
