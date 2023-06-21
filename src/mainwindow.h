/*
    SPDX-FileCopyrightText: Nate Rogers <nate.rogers@kdab.com>
    SPDX-FileCopyrightText: Milian Wolff <milian.wolff@kdab.com>
    SPDX-FileCopyrightText: 2016-2022 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QScopedPointer>
#include <QString>

#include <KParts/MainWindow>
#include <KSharedConfig>

namespace Ui {
class MainWindow;
}

class PerfParser;
class QStackedWidget;

class KRecentFilesAction;

class StartPage;
class ResultsPage;
class ResultsPageDiff;
class RecordPage;
class SettingsDialog;
class DiffReportDialog;

class MainWindow : public KParts::MainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    void clear();
    void openFile(const QString& path);
    void openFile(const QUrl& url);
    void reload();
    void saveAs();
    void saveAs(const QUrl& url);
    void saveAs(const QString& path, const QUrl& url);

    void onOpenFileButtonClicked();
    void onRecordButtonClicked();
    void onHomeButtonClicked();

    void aboutKDAB();
    void openSettingsDialog();
    void aboutHotspot();

    void setCodeNavigationIDE(QAction* action);
    void navigateToCode(const QString& url, int lineNumber, int columnNumber);

    static void openInNewWindow(const QString& file, const QStringList& args = {});

signals:
    void openFileError(const QString& errorMessage);
    void exportFinished(const QUrl& url);
    void exportFailed(const QString& errorMessage);

private:
    void clear(bool isReload);
    void openFile(const QString& path, bool isReload);
    void closeEvent(QCloseEvent* event) override;
    void setupCodeNavigationMenu();
    QString queryOpenDataFile();

    QScopedPointer<Ui::MainWindow> ui;
    PerfParser* m_parser;
    KSharedConfigPtr m_config;
    QStackedWidget* m_pageStack;
    StartPage* m_startPage;
    RecordPage* m_recordPage;
    ResultsPage* m_resultsPage;
    ResultsPageDiff* m_resultsPageDiff;
    SettingsDialog* m_settingsDialog;
    DiffReportDialog* m_diffReportDialog;

    KRecentFilesAction* m_recentFilesAction = nullptr;
    QAction* m_reloadAction = nullptr;
    QAction* m_exportAction = nullptr;
};
