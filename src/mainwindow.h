/*
  mainwindow.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Nate Rogers <nate.rogers@kdab.com>

  Licensees holding valid commercial KDAB Hotspot licenses may use this file in
  accordance with Hotspot Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <QMainWindow>
#include <QScopedPointer>
#include <QString>

#include <KSharedConfig>

namespace Ui {
class MainWindow;
}

namespace Data {
struct Symbol;
}

class PerfParser;
class CallerCalleeModel;
class QSortFilterProxyModel;
class QTreeView;
class KRecentFilesAction;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void setSysroot(const QString& path);
    void setKallsyms(const QString& path);
    void setDebugPaths(const QString& paths);
    void setExtraLibPaths(const QString& paths);
    void setAppPath(const QString& path);
    void setArch(const QString& arch);

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

public slots:
    void clear();
    void openFile(const QString& path);
    void openFile(const QUrl& url);

    void aboutKDAB();
    void aboutHotspot();

private slots:
    void on_openFileButton_clicked();

    void customContextMenu(const QPoint &point, QTreeView* view, int symbolRole);
    void onBottomUpContextMenu(const QPoint &pos);
    void onTopDownContextMenu(const QPoint &pos);
    void jumpToCallerCallee(const Data::Symbol &symbol);

    void navigateToCode(const QString &url, int lineNumber, int columnNumber);
    void setCodeNavigationIDE(QAction *action);

    void onSourceMapContextMenu(const QPoint &pos);

private:
    void showError(const QString& errorMessage);
    void updateBackground();
    void setupCodeNavigationMenu();
    void setupPathSettingsMenu();

    QScopedPointer<Ui::MainWindow> ui;
    PerfParser* m_parser;
    CallerCalleeModel* m_callerCalleeCostModel;
    QSortFilterProxyModel* m_callerCalleeProxy;
    QString m_sysroot;
    QString m_kallsyms;
    QString m_debugPaths;
    QString m_extraLibPaths;
    QString m_appPath;
    QString m_arch;
    QPixmap m_background;
    KRecentFilesAction* m_recentFilesAction = nullptr;
    KSharedConfigPtr m_config;
};
