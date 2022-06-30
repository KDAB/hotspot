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
class Repository;
}

class Highlighter : public QObject
{
    Q_OBJECT
public:
    Highlighter(QTextDocument* document, QObject* parent = nullptr);
    ~Highlighter();

    void setDefinitionForFilename(const QString& filename);
    void setDefinitionForName(const QString& name);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateColorTheme();

#if KF5SyntaxHighlighting_FOUND
    std::unique_ptr<KSyntaxHighlighting::Repository> m_repository;
    KSyntaxHighlighting::SyntaxHighlighter* m_highlighter = nullptr;
#endif
};
