/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

class PerfOutputWidget : public QWidget
{
    Q_OBJECT
public:
    PerfOutputWidget(QWidget* parent = nullptr);
    virtual ~PerfOutputWidget();

    virtual void addOutput(const QString&) = 0;
    virtual void clear() = 0;
    virtual void enableInput(bool enable) = 0;
    virtual void setInputVisible(bool visible) = 0;

signals:
    void sendInput(const QByteArray& input);
};
