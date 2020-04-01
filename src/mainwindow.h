/*
  mainwindow.h

  This file is part of Hotspot, the Qt GUI for performance analysis.

  Copyright (C) 2016-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
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

class PerfParser;
class QStackedWidget;

class KRecentFilesAction;

class StartPage;
class ResultsPage;
class RecordPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    void setSysroot(const QString& path);
    void setKallsyms(const QString& path);
    void setDebugPaths(const QString& paths);
    void setExtraLibPaths(const QString& paths);
    void setAppPath(const QString& path);
    void setArch(const QString& arch);

    void clear();
    void openFile(const QString& path);
    void openFile(const QUrl& url);
    void reload();

    void onOpenFileButtonClicked();
    void onRecordButtonClicked();
    void onHomeButtonClicked();

    void aboutKDAB();
    void aboutHotspot();

    void setCodeNavigationIDE(QAction* action);
    void navigateToCode(const QString& url, int lineNumber, int columnNumber);

signals:
    void openFileError(const QString& errorMessage);
    void sysrootChanged(const QString& path);
    void kallsymsChanged(const QString& path);
    void debugPathsChanged(const QString& paths);
    void extraLibPathsChanged(const QString& paths);
    void appPathChanged(const QString& path);
    void archChanged(const QString& arch);

private:
    void clear(bool isReload);
    void openFile(const QString& path, bool isReload);
    void closeEvent(QCloseEvent* event) override;
    void setupCodeNavigationMenu();
    void setupPathSettingsMenu();

    QScopedPointer<Ui::MainWindow> ui;
    PerfParser* m_parser;
    KSharedConfigPtr m_config;
    QStackedWidget* m_pageStack;
    StartPage* m_startPage;
    RecordPage* m_recordPage;
    ResultsPage* m_resultsPage;

    QString m_sysroot;
    QString m_kallsyms;
    QString m_debugPaths;
    QString m_extraLibPaths;
    QString m_appPath;
    QString m_arch;
    KRecentFilesAction* m_recentFilesAction = nullptr;
    QAction* m_reloadAction = nullptr;
};
