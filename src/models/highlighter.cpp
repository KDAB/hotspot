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

#if KFSyntaxHighlighting_FOUND
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#endif

Highlighter::Highlighter(QTextDocument* document, KSyntaxHighlighting::Repository* repository, QObject* parent)
    : QObject(parent)
#if KFSyntaxHighlighting_FOUND
    , m_highlighter(new KSyntaxHighlighting::SyntaxHighlighter(document))
    , m_repository(repository)
#endif
{
#if !KFSyntaxHighlighting_FOUND
    Q_UNUSED(repository);
#endif
    document->setDefaultFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    // if qApp is used, UBSAN complains
    QApplication::instance()->installEventFilter(this);

    updateColorTheme();
}

Highlighter::~Highlighter() = default;

void Highlighter::setDefinition(const KSyntaxHighlighting::Definition& definition)
{
#if KFSyntaxHighlighting_FOUND
    // don't reparse if definition hasn't changed
    if (m_currentDefinition == definition.name())
        return;

    m_highlighter->setDefinition(definition);
    m_currentDefinition = definition.name();
    emit definitionChanged(m_currentDefinition);
#else
    Q_UNUSED(definition);
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
#if KFSyntaxHighlighting_FOUND
    if (!m_repository) {
        return;
    }

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
