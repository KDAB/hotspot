/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "perfoutputwidgettext.h"

#include <QLineEdit>
#include <QRegularExpression>
#include <QTextEdit>
#include <QVBoxLayout>

PerfOutputWidgetText::PerfOutputWidgetText(QWidget* parent)
    : PerfOutputWidget(parent)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    m_perfOutputTextEdit = new QTextEdit(this);
    m_perfOutputTextEdit->setReadOnly(true);
    m_perfOutputTextEdit->setPlaceholderText(tr("Waiting for recording to start..."));
    m_perfOutputTextEdit->setFontFamily(QStringLiteral("Monospace"));
    layout->addWidget(m_perfOutputTextEdit);

    m_perfInputEdit = new QLineEdit(this);
    m_perfInputEdit->setEnabled(false);
    m_perfInputEdit->setPlaceholderText(tr("send input to process..."));
    layout->addWidget(m_perfInputEdit);

    connect(m_perfInputEdit, &QLineEdit::returnPressed, this, [this]() {
        emit sendInput(m_perfInputEdit->text().toUtf8() + QByteArrayLiteral("\n"));
        m_perfInputEdit->clear();
    });
}

PerfOutputWidgetText::~PerfOutputWidgetText() = default;

void PerfOutputWidgetText::addOutput(const QString& output)
{
    // this regex finds ansi escapes
    static const QRegularExpression regex(QStringLiteral("\\x1b\\[[0-9;]+m"));

    QTextCursor cursor(m_perfOutputTextEdit->document());
    cursor.movePosition(QTextCursor::End);

    QString cleanOutput = output;
    cursor.insertText(cleanOutput.replace(regex, QString()));
}

void PerfOutputWidgetText::clear()
{
    m_perfInputEdit->clear();
    m_perfOutputTextEdit->clear();
}

void PerfOutputWidgetText::enableInput(bool enable)
{
    m_perfInputEdit->clear();
    m_perfInputEdit->setEnabled(enable);
}

void PerfOutputWidgetText::setInputVisible(bool visible)
{
    m_perfInputEdit->setVisible(visible);
}
