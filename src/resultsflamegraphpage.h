/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>

class QMenu;
class QAction;

namespace Ui {
class ResultsFlameGraphPage;
}

namespace Data {
struct Symbol;
}

class PerfParser;
class FilterAndZoomStack;

class ResultsFlameGraphPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsFlameGraphPage(FilterAndZoomStack* filterStack, PerfParser* parser, QMenu* exportMenu,
                                   QWidget* parent = nullptr);
    ~ResultsFlameGraphPage();

    void clear();

    void setHoveredStacks(const QVector<QVector<Data::Symbol>>& hoveredStacks);

signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);
    void openEditor(const Data::Symbol& symbol);
    void selectSymbol(const Data::Symbol& symbol);
    void selectStack(const QVector<Data::Symbol>& stack, bool bottomUp);
    void jumpToDisassembly(const Data::Symbol& symbol);

private:
    std::unique_ptr<Ui::ResultsFlameGraphPage> ui;
    QAction* m_exportAction = nullptr;
};
