/*
    SPDX-FileCopyrightText: Lieven Hey <lieven.hey@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "data.h"

#include <QWidget>

#include <memory>

class PerfParser;
class QCustomPlot;

namespace Ui {
class FrequencyPage;
}

class FrequencyPage : public QWidget
{
    Q_OBJECT
public:
    FrequencyPage(PerfParser* parser, QWidget* parent = nullptr);
    ~FrequencyPage();

protected:
    void changeEvent(QEvent* event) override;

private:
    void updateColors();

    QCustomPlot* m_plot = nullptr;
    std::unique_ptr<Ui::FrequencyPage> m_page;
    Data::FrequencyResults m_results;

    double m_upperWithoutOutliers = 0;
};
