/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>

class QMenu;

namespace Ui {
class ResultsBottomUpPage;
}

namespace Data {
struct Symbol;
}

class QTreeView;

class PerfParser;
class FilterAndZoomStack;
class CostContextMenu;

class ResultsBottomUpPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsBottomUpPage(FilterAndZoomStack* filterStack, PerfParser* parser, CostContextMenu* contextMenu,
                                 QMenu* exportMenu, QWidget* parent = nullptr);
    ~ResultsBottomUpPage();

    void clear();

signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);
    void jumpToDisassembly(const Data::Symbol& symbol);
    void openEditor(const Data::Symbol& symbol);
    void selectSymbol(const Data::Symbol& symbol);

private:
    std::unique_ptr<Ui::ResultsBottomUpPage> ui;
};
