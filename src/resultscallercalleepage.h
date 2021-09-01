/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>

namespace Ui {
class ResultsCallerCalleePage;
}

namespace Data {
struct Symbol;
}

class QSortFilterProxyModel;
class QModelIndex;

class PerfParser;
class CallerCalleeModel;
class FilterAndZoomStack;
class CostContextMenu;
class CallgraphWidget;

class ResultsCallerCalleePage : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsCallerCalleePage(FilterAndZoomStack* filterStack, PerfParser* parser, CostContextMenu* contextMenu,
                                     QWidget* parent = nullptr);
    ~ResultsCallerCalleePage();

    void setSysroot(const QString& path);
    void setAppPath(const QString& path);
    void clear();

    void jumpToCallerCallee(const Data::Symbol& symbol);
    void openEditor(const Data::Symbol& symbol);

private slots:
    void onSourceMapContextMenu(const QPoint& pos);
    void onSourceMapActivated(const QModelIndex& index);

signals:
    void navigateToCode(const QString& url, int lineNumber, int columnNumber);
    void navigateToCodeFailed(const QString& message);
    void selectSymbol(const Data::Symbol& symbol);
    void jumpToDisassembly(const Data::Symbol& symbol);

private:
    struct SourceMapLocation
    {
        inline explicit operator bool() const
        {
            return !path.isEmpty();
        }

        QString path;
        int lineNumber = -1;
    };
    SourceMapLocation toSourceMapLocation(const QModelIndex& index) const;
    SourceMapLocation toSourceMapLocation(const QString& location, const Data::Symbol& symbol) const;

    QScopedPointer<Ui::ResultsCallerCalleePage> ui;
    CallgraphWidget* m_callgraph;

    CallerCalleeModel* m_callerCalleeCostModel;
    QSortFilterProxyModel* m_callerCalleeProxy;

    QString m_sysroot;
    QString m_appPath;
};
