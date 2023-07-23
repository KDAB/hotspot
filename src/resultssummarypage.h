/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>

namespace Data {
struct Symbol;
}

namespace Ui {
class ResultsSummaryPage;
}

class PerfParser;
class FilterAndZoomStack;
class CostContextMenu;

class ResultsSummaryPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsSummaryPage(FilterAndZoomStack* filterStack, PerfParser* parser, CostContextMenu* contextMenu,
                                QWidget* parent = nullptr);
    ~ResultsSummaryPage();

signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);
    void openEditor(const Data::Symbol& symbol);
    void selectSymbol(const Data::Symbol& symbol);
    void jumpToDisassembly(const Data::Symbol& symbol);

private:
    std::unique_ptr<Ui::ResultsSummaryPage> ui;
};
