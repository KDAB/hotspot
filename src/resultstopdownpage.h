/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

namespace Ui {
class ResultsTopDownPage;
}

namespace Data {
struct Symbol;
struct TopDownResults;
}

class QTreeView;

class PerfParser;
class FilterAndZoomStack;
class CostContextMenu;
class TopDownModel;

class ResultsTopDownPage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsTopDownPage(FilterAndZoomStack* filterStack, PerfParser* parser, CostContextMenu* contextMenu,
                                QWidget* parent = nullptr);
    ~ResultsTopDownPage();

    void clear();

public slots:
    void setTopDownResults(const Data::TopDownResults& data);

signals:
    void jumpToCallerCallee(const Data::Symbol& symbol);
    void openEditor(const Data::Symbol& symbol);
    void selectSymbol(const Data::Symbol& symbol);
    void jumpToDisassembly(const Data::Symbol& symbol);

private:
    TopDownModel* m_model = nullptr;
    QScopedPointer<Ui::ResultsTopDownPage> ui;
};
