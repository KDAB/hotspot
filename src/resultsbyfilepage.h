/*
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

#include <memory>

namespace Ui {
class ResultsByFilePage;
}

namespace Data {
struct FileLine;
}

class QSortFilterProxyModel;

class PerfParser;
class ByFileModel;
class FilterAndZoomStack;
class CostContextMenu;

class ResultsByFilePage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsByFilePage(FilterAndZoomStack* filterStack, PerfParser* parser, CostContextMenu* contextMenu,
                               QWidget* parent = nullptr);
    ~ResultsByFilePage();

    void clear();

signals:
    void openFileLineRequested(const Data::FileLine& fileLine);

private:
    void onSourceMapContextMenu(QPoint point);

    std::unique_ptr<Ui::ResultsByFilePage> ui;

    ByFileModel* m_byFileCostModel;
    QSortFilterProxyModel* m_byFileProxy;
};
