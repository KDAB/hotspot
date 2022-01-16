/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PERFOUTPUTWIDGETTEXT_H
#define PERFOUTPUTWIDGETTEXT_H

#include "perfoutputwidget.h"
#include <QWidget>

class QTemporaryFile;
class QTextEdit;
class QLineEdit;

class PerfOutputWidgetText : public PerfOutputWidget
{
    Q_OBJECT
public:
    PerfOutputWidgetText(QWidget* parent = nullptr);
    ~PerfOutputWidgetText() override;

    void addOutput(const QString& output) override;
    void clear() override;
    void enableInput(bool enable) override;
    void setInputVisible(bool visible) override;

private:
    QTextEdit* m_perfOutputTextEdit = nullptr;
    QLineEdit* m_perfInputEdit = nullptr;
};

#endif // PERFOUTPUTWIDGETTEXT_H
