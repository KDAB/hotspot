/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

class FrequencyModel;

namespace KChart {
class Chart;
}

class PerfParser;

class FrequencyPage : public QWidget
{
    Q_OBJECT
public:
    FrequencyPage(PerfParser* parser, QWidget* parent = nullptr);
    ~FrequencyPage();

private:
    KChart::Chart* m_widget;
    FrequencyModel* m_model;

protected:
    void changeEvent(QEvent *) override;
};
