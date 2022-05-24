/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "perfoutputwidget.h"

#include <QWidget>

namespace KParts {
class ReadOnlyPart;
}

class QTemporaryFile;

class PerfOutputWidgetKonsole : public PerfOutputWidget
{
    Q_OBJECT
public:
    PerfOutputWidgetKonsole(KParts::ReadOnlyPart* part, QWidget* parent = nullptr);
    ~PerfOutputWidgetKonsole();

    static PerfOutputWidgetKonsole* create(QWidget* parent = nullptr);

    bool eventFilter(QObject* watched, QEvent* event) override;
    void addOutput(const QString& output) override;
    void clear() override;
    void enableInput(bool enable) override;
    void setInputVisible(bool visible) override;

private:
    void addPartToLayout();

    KParts::ReadOnlyPart* m_konsolePart = nullptr;
    QTemporaryFile* m_konsoleFile = nullptr;
    bool m_inputEnabled = false;
    QByteArray m_inputBuffer;
};
