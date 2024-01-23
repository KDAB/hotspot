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
class HighlightedLine;

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

    bool isUsingAnsi() const
    {
        return m_isUsingAnsi;
    }

    // for testing
    QTextLayout* layoutForLine(int index);

signals:
    void definitionChanged(const QString& definition);
    void usesAnsiChanged(bool usesAnsi);

public slots:
    void updateHighlighting();

private:
    KSyntaxHighlighting::Repository* m_repository;
    std::unique_ptr<HighlightingImplementation> m_highlighter;
    std::vector<HighlightedLine> m_highlightedLines;
    QStringList m_lines;
    QStringList m_cleanedLines;
    bool m_isUsingAnsi = false;
};
