/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "perfoutputwidgetkonsole.h"

#include "util.h"

#include <QEvent>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QVBoxLayout>

#include <KParts/ReadOnlyPart>
#include <KParts/kde_terminal_interface.h>

namespace {
QString tailExe()
{
    static const auto tail = QStandardPaths::findExecutable(QStringLiteral("tail"));
    return tail;
}

KParts::ReadOnlyPart* createPart()
{
    return Util::createPart(QStringLiteral("konsolepart"));
}
}

PerfOutputWidgetKonsole::PerfOutputWidgetKonsole(KParts::ReadOnlyPart* part, QWidget* parent)
    : PerfOutputWidget(parent)
    , m_konsolePart(part)
{
    auto layout = new QVBoxLayout(this);
    setLayout(layout);

    addPartToLayout();
}

PerfOutputWidgetKonsole::~PerfOutputWidgetKonsole() = default;

PerfOutputWidgetKonsole* PerfOutputWidgetKonsole::create(QWidget* parent)
{
    if (tailExe().isEmpty()) {
        return nullptr;
    }

    auto part = createPart();
    if (!part) {
        return nullptr;
    }

    const auto terminalInterface = qobject_cast<TerminalInterface*>(part);
    if (!terminalInterface) {
        qWarning("konsole kpart doesn't implement terminal interface");
        delete part;
        return nullptr;
    }

    return new PerfOutputWidgetKonsole(part, parent);
}

bool PerfOutputWidgetKonsole::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);
    if (event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);

        // ignore ctrl + c
        if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers() == Qt::ControlModifier) {
            return true;
        } else if (keyEvent->key() == Qt::Key_S && keyEvent->modifiers() == Qt::ControlModifier) {
            // ignore ctrl + s (stop output)
            return true;
        }

        if (!m_inputEnabled) {
            // eat all key events if input is disabled
            return true;
        } else {
            if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
                emit sendInput(m_inputBuffer + QByteArrayLiteral("\n"));
                m_inputBuffer.clear();
            } else if (keyEvent->key() == Qt::Key_Delete) {
                m_inputBuffer.remove(m_inputBuffer.size() - 1, 1);
            } else {
                m_inputBuffer.append(keyEvent->text().toUtf8());
            }
        }
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto mouseEvent = static_cast<QMouseEvent*>(event);

        // prevent the user from opening the context menu
        if (mouseEvent->button() == Qt::MouseButton::RightButton) {
            return true;
        }
    }
    return false;
}

void PerfOutputWidgetKonsole::addOutput(const QString& output)
{
    m_konsoleFile->write(output.toUtf8());
    m_konsoleFile->flush();
}

void PerfOutputWidgetKonsole::clear()
{
    if (m_konsolePart) {
        m_konsoleFile->deleteLater();
        m_konsolePart->deleteLater();
    }

    m_konsoleFile = nullptr;
    m_konsolePart = createPart();

    addPartToLayout();
}

void PerfOutputWidgetKonsole::enableInput(bool enable)
{
    if (enable != m_inputEnabled) {
        m_inputEnabled = enable;
    }
}

void PerfOutputWidgetKonsole::setInputVisible(bool visible)
{
    Q_UNUSED(visible);
}

void PerfOutputWidgetKonsole::addPartToLayout()
{
    Q_ASSERT(!m_konsoleFile);
    m_konsoleFile = new QTemporaryFile(m_konsolePart);
    m_konsoleFile->open();

    const auto terminalInterface = qobject_cast<TerminalInterfaceV2*>(m_konsolePart);
    terminalInterface->startProgram(tailExe(), {tailExe(), QStringLiteral("-f"), m_konsoleFile->fileName()});

    m_konsolePart->widget()->focusWidget()->installEventFilter(this);

    layout()->addWidget(m_konsolePart->widget());
}
