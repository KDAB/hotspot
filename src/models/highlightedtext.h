/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2023 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <memory>

#include <QObject>

class QTextLayout;
class QTextLine;

#include "hotspot-config.h"

namespace KSyntaxHighlighting {
class SyntaxHighlighter;
class Definition;
class Repository;
}

class HighlightingImplementation;

class HighlightedText : public QObject
{
    Q_OBJECT
public:
    HighlightedText(KSyntaxHighlighting::Repository* repository, QObject* parent = nullptr);
    ~HighlightedText() override;

    void setText(const QStringList& text);
    void setDefinition(const KSyntaxHighlighting::Definition& definition);

    QString textAt(int index) const;
    QTextLine lineAt(int index) const;
    QString definition() const;

signals:
    void definitionChanged(const QString& definition);

private slots:
    void applyFormatting();

private:
    void updateHighlighting();

#if KFSyntaxHighlighting_FOUND
    KSyntaxHighlighting::Repository* m_repository;
#endif
    std::unique_ptr<HighlightingImplementation> m_highlighter;
    std::unique_ptr<QTextLayout> m_layout;
    QStringList m_lines;
    QStringList m_cleanedLines;
};
