/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: 2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "hotspot-config.h"

#include <QObject>

#include <memory>

class QTextDocument;

namespace KSyntaxHighlighting {
class SyntaxHighlighter;
class Definition;
class Repository;
}

class Highlighter : public QObject
{
    Q_OBJECT
public:
    Highlighter(QTextDocument* document, KSyntaxHighlighting::Repository* repository, QObject* parent = nullptr);
    ~Highlighter();

    void setDefinition(const KSyntaxHighlighting::Definition& definition);

    QString definition() const
    {
#if KFSyntaxHighlighting_FOUND
        return m_currentDefinition;
#else
        return {};
#endif
    }

signals:
    void definitionChanged(const QString& definition);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateColorTheme();

#if KFSyntaxHighlighting_FOUND
    KSyntaxHighlighting::SyntaxHighlighter* m_highlighter = nullptr;
    KSyntaxHighlighting::Repository* m_repository = nullptr;
    QString m_currentDefinition;
#endif
};
