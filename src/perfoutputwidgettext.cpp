/*
  perfoutputwidgettext.cpp

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2021 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Lieven Hey <lieven.hey@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
